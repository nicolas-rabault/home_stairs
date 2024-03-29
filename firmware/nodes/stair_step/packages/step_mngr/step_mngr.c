/******************************************************************************
 * @file lightning manager
 * @brief an application managin one step lightning of a stair
 * @author Nicolas Rabault
 * @version 1.0.0
 ******************************************************************************/
#include "step_mngr.h"
#include "product_config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define UPDATE_PERIOD_MS  20                                  // Sensor update period
#define FILTER_STENGTH    0.1                                 // Filtering strength of the sensor
#define STAIR_LENGHT      2.2                                 // Stairs length meters
#define STEP_NUMBER       11                                  // Number of step on the stairs
#define STEP_WIDTH        0.69                                // Width of a step in meter
#define WAVE_SPEED        3                                   // Propagation speed of the waves meter/seconds
#define FRAME_RATE_MS     10                                  // Frame rate calculation
#define INITIAL_GAIN      0.1                                 // Base gain between force value and light intensity
#define OFFSET            1500                                // This offset should be bigger than the filtered noise of the sensor.
#define MAX_LIGHT_VALUE   100                                 // This max value will be used to auto-scale the FORCE_LIGHT_SCALING
#define LED_NBR           39                                  // Number of led per step
#define LIGHT_TRESHOLD    0                                   // The minimal weight needed to trigger a wave
#define SPACE_BETWEEN_LED (STEP_WIDTH / LED_NBR)              // Space between leds
#define ANIMATION_SMOOTH  4                                   // animation smothness should be %2
#define DIST_RES          SPACE_BETWEEN_LED *ANIMATION_SMOOTH // Animation resolution meters
#define ANIM_SIZE         125 * ANIMATION_SMOOTH              // Size of the animation table. I have to calculate it myself, don't know why((const int)(STAIR_LENGHT / SPACE_BETWEEN_LED))
/*******************************************************************************
 * Variables
 ******************************************************************************/
service_t *app;
volatile control_t control_app;
volatile force_t raw_force         = 0.0;
float force_light_scaling          = INITIAL_GAIN;
volatile illuminance_t light_force = 0.0;
volatile force_t filtered_force    = 0.0;

#ifdef DETECTOR
uint32_t boot_time = 0;
#endif

// A representation of all the leds distance from the sensor of this step.
// In this table we will save the ID of the delta_intentisty table.
uint16_t slot_map[STEP_NUMBER][LED_NBR] = {0};

/*******************************************************************************
 * Function
 ******************************************************************************/
static void frame_transmit(uint8_t *light_intensity);
void compute_slot_map(void);

static void StepMngr_MsgHandler(service_t *service, msg_t *msg)
{
    static bool gate_feedback = false;
    if (msg->header.cmd == END_DETECTION)
    {
        // We receive an end_detection, we need to initialize the sensor service to auto-update
        gate_feedback = false;
        if (Luos_IsNodeDetected())
        {
            // We need to control the local sensor if it exists, search if there is a local load sensor
            uint16_t my_nodeid = RoutingTB_NodeIDFromID(service->ll_service->id);
            search_result_t filter_result;
            RTFilter_Reset(&filter_result);
            RTFilter_Type(&filter_result, LOAD_TYPE);
            RTFilter_Node(&filter_result, my_nodeid);

            if (filter_result.result_nbr == 1)
            {
                // Auto-update - ask to send its value each UPDATE_PERIOD_MS
                msg_t send_msg;
                send_msg.header.target      = filter_result.result_table[0]->id;
                send_msg.header.target_mode = IDACK;

                // Setup auto update each UPDATE_PERIOD_MS on load
                // This value is resetted on all service at each detection
                // It's important to setting it each time.
                time_luos_t time = TimeOD_TimeFrom_ms(UPDATE_PERIOD_MS);
                TimeOD_TimeToMsg(&time, &send_msg);
                send_msg.header.cmd = UPDATE_PUB;
                while (Luos_SendMsg(service, &send_msg) != SUCCEED)
                    ;
            }
            RTFilter_Reset(&filter_result);
            RTFilter_Type(&filter_result, COLOR_TYPE);
            RTFilter_Node(&filter_result, my_nodeid);
            if (filter_result.result_nbr == 1)
            {
                // Turn of my local led strip
                msg_t send_msg;
                send_msg.header.target      = filter_result.result_table[0]->id;
                send_msg.header.target_mode = IDACK;
                send_msg.header.size        = 3;
                send_msg.header.cmd         = COLOR;
                send_msg.data[0]            = 0;
                send_msg.data[1]            = 0;
                send_msg.data[2]            = 0;
                while (Luos_SendMsg(service, &send_msg) != SUCCEED)
                    ;
            }
            // To finish we have to compute the new slot_map
            compute_slot_map();
        }
    }

    if (msg->header.cmd == FORCE)
    {
        // We receive a force value from our local load sensor
        ForceOD_ForceFromMsg((force_t *)&raw_force, msg);
    }

    if (msg->header.cmd == CONTROL)
    {
        // We receive a control message from the gate to stop or start the animation
        control_app.unmap = msg->data[0];
        return;
    }

    if (msg->header.cmd == GET_CMD)
    {
        // We receive a get command from the gate, we need to send the current state of the app
        if (gate_feedback == false)
        {
            // We have to setup things for the gate for the first execution.
            gate_feedback = true;
            if (Luos_IsNodeDetected())
            {
                // We need to control the local sensor if it exists, search if there is a local load sensor
                uint16_t my_nodeid = RoutingTB_NodeIDFromID(service->ll_service->id);
                search_result_t filter_result;
                RTFilter_Reset(&filter_result);
                RTFilter_Type(&filter_result, LOAD_TYPE);
                RTFilter_Node(&filter_result, my_nodeid);

                if (filter_result.result_nbr == 1)
                {
                    // Auto-update - ask to send its value each UPDATE_PERIOD_MS
                    msg_t send_msg;
                    send_msg.header.target      = filter_result.result_table[0]->id;
                    send_msg.header.target_mode = IDACK;

                    // Setup auto update each UPDATE_PERIOD_MS on load
                    // This value is resetted on all service at each detection
                    // It's important to setting it each time.
                    time_luos_t time = TimeOD_TimeFrom_ms(UPDATE_PERIOD_MS);
                    TimeOD_TimeToMsg(&time, &send_msg);
                    send_msg.header.cmd = UPDATE_PUB;
                    while (Luos_SendMsg(service, &send_msg) != SUCCEED)
                        ;
                }
            }
        }
        msg_t pub_msg;
        // Fill the message infos
        pub_msg.header.target_mode = ID;
        pub_msg.header.target      = msg->header.source;
        // Send back the calculated light intensity
        IlluminanceOD_IlluminanceToMsg((illuminance_t *)&light_force, &pub_msg);
        Luos_SendMsg(service, &pub_msg);

        // Send back the filtered force
        ForceOD_ForceToMsg((force_t *)&filtered_force, &pub_msg);
        Luos_SendMsg(service, &pub_msg);
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
    control_app.flux = STOP;
    // Create App
    app = Luos_CreateService(StepMngr_MsgHandler, STEP_APP, "step", revision);

#ifdef DETECTOR
    boot_time = Luos_GetSystick();
#endif
}

void StepMngr_Loop(void)
{
#ifdef DETECTOR
    // Perform a detection at boot if we are in detector mode
    static bool detection_done = false;
    if (((Luos_GetSystick() - boot_time) > 1000) & !detection_done)
    {
        Luos_Detect(app);
        detection_done = true;
        return;
    }
#endif
    /*
     * This table represent the wave intensity per distance with a DIST_RES granularity with
     * a max distance of STAIR_LENGHT.
     */
    static uint8_t light_intensity[ANIM_SIZE] = {0};
    static uint32_t last_animation_date       = 0;
    static uint32_t last_frame_date           = 0;
#ifdef DETECTOR
    static force_t force = (MAX_LIGHT_VALUE - OFFSET) / INITIAL_GAIN;
#else
    static force_t force = 0.0;
#endif

    // Check Animation timing : time = distance/speed
    if ((Luos_GetSystick() - last_animation_date >= ((uint32_t)((DIST_RES / (WAVE_SPEED * ANIMATION_SMOOTH)) * 1000.0))) && !(control_app.flux == STOP))
    {
        /*
         * We have to insert weight values at light_intensity slot 0.
         * Then we have to move the values from our position to the end of the table
         */
        // Move values from 0 to the table end
        for (int i = ANIM_SIZE - 1; i > 0; i--)
        {
            light_intensity[i] = light_intensity[i - 1];
        }
        // Low pass filtering on the sensor value
        force = force + ((raw_force - force) * FILTER_STENGTH);
        // Consider absolute values allowing to deal with any direction of the sensor and offset it to remove noise area.
        force = (float)(fabs((double)force) - OFFSET);
        // Save the value allowing to send it back to a Gate
        filtered_force = force;
        // Then compute the new intensity and insert it into the animation table
        float value = force * force_light_scaling;
        // Auto-scale the light intensity clamped to max value
        if (value > MAX_LIGHT_VALUE)
        {
            value               = MAX_LIGHT_VALUE;
            force_light_scaling = MAX_LIGHT_VALUE / force;
        }
        LUOS_ASSERT(value <= 255.0f);
        // Write the new value and clamp it to 0
        if ((value) < 1.0f)
        {
            light_intensity[0] = 0;
            // Reset the gain allowing to maximize the light intensity for any new usage
            force_light_scaling = INITIAL_GAIN;
        }
        else
        {
            light_intensity[0] = (uint8_t)value;
        }
        // Save the value allowing to send it back to a Gate
        light_force         = (float)light_intensity[0];
        last_animation_date = Luos_GetSystick();
    }

    // Check Frame timing
    if (((Luos_GetSystick() - last_frame_date) >= FRAME_RATE_MS) && (control_app.flux == PLAY))
    {
        /*
         * We have to compute a frame
         * Frames are computed from the light_intensity table
         * The idea is to send the light_intensity information to the leds corresponding to the distance.
         */
        frame_transmit(light_intensity);
        last_frame_date = Luos_GetSystick();
    }
}

void frame_transmit(uint8_t *light_intensity)
{
    // Real led light intensity value
    static uint8_t frame[STEP_NUMBER][LED_NBR] = {0};
    /*
     * To compute a frame we have to :
     * - parse all the leds step by step
     * - compute the intensity depending on the corresponding value of the slot of slot_map
     * - send the leds informations at each steps
     */

    // Find the step apps by getting a list off all available apps
    search_result_t filter_result;
    RTFilter_Reset(&filter_result);
    RTFilter_Type(&filter_result, COLOR_TYPE);

    // Parse all the steps
    for (int step_index = 0; step_index < filter_result.result_nbr; step_index++) // Step
    {
        // Delta light intensity
        int8_t delta_frame[LED_NBR] = {0};
        // Parse all the led fo each steps
        bool send_it = false;
        for (int led_index = 0; led_index < LED_NBR; led_index++) // Led
        {
            // Compute the delta intensity of this led
            LUOS_ASSERT(abs((int)light_intensity[slot_map[step_index][led_index]] - (int)frame[step_index][led_index]) <= 128);
            delta_frame[led_index] = (int)light_intensity[slot_map[step_index][led_index]] - (int)frame[step_index][led_index];

            frame[step_index][led_index] = light_intensity[slot_map[step_index][led_index]];
            if (delta_frame[led_index] != 0)
            {
                // We will have to send a message for this step
                send_it = true;
            }
        }
        if (send_it)
        {
            // We have some information to transmit for this step
            msg_t msg;
            // step_index should be the index of the concerned app
            msg.header.target      = filter_result.result_table[step_index]->id;
            msg.header.target_mode = IDACK;
            msg.header.cmd         = DELTA_COLOR;
            msg.header.size        = sizeof(delta_frame);
            memcpy(msg.data, delta_frame, sizeof(delta_frame));
            while (Luos_SendMsg(app, &msg) != SUCCEED)
                ;
        }
    }
}

void compute_slot_map(void)
{
    /*
     * To compute the slot_map we have to :
     * - parse all the leds step by step
     * - compute the distance of this led from the local wave center
     * - get the slot on light_intensity correponding to the distance of this led
     * - save it on the slot_map table
     */

    /* First compute the position of the wave source (basicaly it depend on the physical position of our current step)
     * First define the step position.
     * To do that we have to make the first step do the detection to have a deterministic numbers on steps
     * This way the (node_id - 1) represent the step number
     */
#ifdef DETECTOR
    color_t color_dist[LED_NBR] = {0};
    const float phase2          = 2.0f * M_PI / 3.0f;
    const float phase3          = 4.0f * M_PI / 3.0f;
#endif
    uint16_t my_nodeid = RoutingTB_NodeIDFromID(app->ll_service->id);
    search_result_t color_list;
    RTFilter_Reset(&color_list);
    RTFilter_Type(&color_list, COLOR_TYPE);

    // Find the local one
    search_result_t local_color = color_list;
    RTFilter_Node(&local_color, my_nodeid);

    LUOS_ASSERT(local_color.result_nbr > 0);
    // Now we can define the position of the local one.
    int my_step_position = 0;
    while (color_list.result_table[my_step_position]->id != local_color.result_table[0]->id)
    {
        my_step_position++;
    }

    const float wave_center_x = (((STAIR_LENGHT / STEP_NUMBER) * my_step_position));
    const float wave_center_y = STEP_WIDTH / 2.0;

    // Parse all the steps
    for (int step_index = 0; step_index < STEP_NUMBER; step_index++) // Step
    {
        // Compute the x distance of this step
        const float color_x = step_index * (STAIR_LENGHT / STEP_NUMBER);
        const float x       = color_x - wave_center_x;
        // Parse all the led fo each steps
        for (int led_index = 0; led_index < LED_NBR; led_index++) // Led
        {
            // Compute the y distance of this led
            const float color_y = led_index * SPACE_BETWEEN_LED;
            const float y       = color_y - wave_center_y;
            // Compute the distance of this led from the wave source
            double distance = sqrt(((x) * (x)) + ((y) * (y)));
            // Depending on this distance define the corresponding slot on light_intensity and save this slot in the slot_map
            slot_map[step_index][led_index] = (uint16_t)(distance / DIST_RES);
#ifdef DETECTOR
            double color_distance = sqrt(((color_x) * (color_x)) + ((color_y) * (color_y)));
            // Precompute the Color distribution
            color_dist[led_index].r = (uint8_t)(255 * sin(color_distance / STAIR_LENGHT));
            color_dist[led_index].g = (uint8_t)(255 * sin(color_distance / STAIR_LENGHT - phase2));
            color_dist[led_index].b = (uint8_t)(255 * sin(color_distance / STAIR_LENGHT - phase3));
#endif
        }
#ifdef DETECTOR
        // Send the color distribution to this step
        msg_t msg;
        // step_index should be the index of the concerned app
        msg.header.target      = color_list.result_table[step_index]->id;
        msg.header.target_mode = IDACK;
        msg.header.cmd         = PARAMETERS;
        msg.header.size        = sizeof(color_dist);
        memcpy(msg.data, color_dist, sizeof(color_dist));
        while (Luos_SendMsg(app, &msg) != SUCCEED)
            ;
#endif
    }
    Luos_Loop();
    // We are ready to compute the light intensity
    control_app.flux = PLAY;
}