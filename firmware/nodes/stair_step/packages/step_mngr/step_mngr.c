/******************************************************************************
 * @file lightning manager
 * @brief an application managin one step lightning of a stair
 * @author Nicolas Rabault
 * @version 1.0.0
 ******************************************************************************/
#include "lightning_manager.h"
#include "product_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define UPDATE_PERIOD_MS 20
/*******************************************************************************
 * Variables
 ******************************************************************************/
service_t *app;
volatile control_t control_app;

/*******************************************************************************
 * Function
 ******************************************************************************/
static void StepMngr_MsgHandler(service_t *service, msg_t *msg) {

    if (msg->header.cmd == IO_STATE)
    {
        if (control_app.flux == PLAY)
        {
            if (RoutingTB_TypeFromID(msg->header.source) == STATE_TYPE)
            {
                // this is the button reply we have filter it to manage monostability
                if ((!last_btn_state) & (last_btn_state != msg->data[0]))
                {
                    lock = (!lock);
                    state_switch++;
                }
            }
            else
            {
                // this is an already filtered information
                if ((lock != msg->data[0]))
                {
                    lock = msg->data[0];
                    state_switch++;
                }
            }
            last_btn_state = msg->data[0];
        }
        return;
    }
    if (msg->header.cmd == CONTROL)
    {
        control_app.unmap = msg->data[0];
        return;
    }
}

/******************************************************************************
 * @brief init must be call in project init
 * @param None
 * @return None
 ******************************************************************************/
void StepMngr_Init(void)
{
    revision_t revision = {.major = 1, .minor = 0, .build = 0};
    // By default this app running
    control_app.flux = PLAY;
    // Create App
    app = Luos_CreateService(StepMngr_MsgHandler, STEP_APP, "step", revision);
#ifdef DETECTOR
    while (Luos_GetSystick() < 100)
        ;
    RoutingTB_DetectServices(app);
#endif
}

void StepMngr_Loop(void)
{
    static short previous_id       = -1;
    static uint32_t detection_date = 0;
    // ********** hot plug management ************
    // Check if we have done the first init or if service Id have changed
    if (previous_id != RoutingTB_IDFromService(app))
    {
        if (RoutingTB_IDFromService(app) == 0)
        {
            // We don't have any ID, meaning no detection occure or detection is occuring.
            // someone is making a detection, let it finish.
            // reset the init state to be ready to setup service at the end of detection
            previous_id    = 0;
            detection_date = Luos_GetSystick();
        }
        else
        {
            if ((Luos_GetSystick() - detection_date) > 100)
            {
                // Make services configurations
                // Find the local load sensor
                int id = RoutingTB_IDFromClosestType(app, LOAD_TYPE);
                if (id > 0)
                {
                    msg_t msg;
                    msg.header.target      = id;
                    msg.header.target_mode = IDACK;
                    // Setup auto update each UPDATE_PERIOD_MS on button
                    // This value is resetted on all service at each detection
                    // It's important to setting it each time.
                    time_luos_t time = TimeOD_TimeFrom_ms(UPDATE_PERIOD_MS);
                    TimeOD_TimeToMsg(&time, &msg);
                    msg.header.cmd = UPDATE_PUB;
                    while (Luos_SendMsg(app, &msg) != SUCCEED)
                    {
                        Luos_Loop();
                    }
                }
                previous_id = RoutingTB_IDFromService(app);
            }
        }
        return;
    }
}