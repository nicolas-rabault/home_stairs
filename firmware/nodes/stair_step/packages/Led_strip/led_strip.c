/******************************************************************************
 * @file led strip
 * @brief driver example a simple led strip
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#include "led_strip.h"
#include "led_strip_drv.h"
#include "product_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
color_t matrix[MAX_LED_NUMBER];
int imgsize = MAX_LED_NUMBER;

/*******************************************************************************
 * Function
 ******************************************************************************/
static void LedStrip_MsgHandler(service_t *service, msg_t *msg);

/******************************************************************************
 * @brief init must be call in project init
 * @param None
 * @return None
 ******************************************************************************/
void LedStrip_Init(void)
{
    revision_t revision = {.major = 1, .minor = 0, .build = 0};
    // create led strip service
    Luos_CreateService(LedStrip_MsgHandler, COLOR_TYPE, "led_strip", revision);
    // initialize color matrix with 0
    memset((void *)matrix, 0, MAX_LED_NUMBER * 3);
    // initialize driver
    LedStripDrv_Init();
}
/******************************************************************************
 * @brief loop must be call in project loop
 * @param None
 * @return None
 ******************************************************************************/
void LedStrip_Loop(void)
{
    // write in buffer transfered through dma
    LedStripDrv_Write(matrix);
}
/******************************************************************************
 * @brief Msg Handler call back when a msg receive for this service
 * @param Service destination
 * @param Msg receive
 * @return None
 ******************************************************************************/
static void LedStrip_MsgHandler(service_t *service, msg_t *msg)
{
    if (msg->header.cmd == COLOR)
    {
        // change led target color
        if (msg->header.size == 3)
        {
            // there is only one color copy it in the entire matrix
            for (int i = 0; i < imgsize; i++)
            {
                memcpy((void *)matrix + i, msg->data, sizeof(color_t));
            }
        }
        else
        {
            // image management
            Luos_ReceiveData(service, msg, (void *)matrix);
        }
        return;
    }
    if (msg->header.cmd == PARAMETERS)
    {
        // set the led strip size
        short size;
        memcpy(&size, msg->data, sizeof(short));
        // resize by puting 0 in the end of the led strip
        memset((void *)matrix + size, 0, (MAX_LED_NUMBER - size) * 3);
        imgsize = size;
        return;
    }
    if (msg->header.cmd == DELTA_COLOR)
    {
        // This is a custom derivative form of the picture allowing concurent access to the image by adding it.
        // Check size
        LUOS_ASSERT(msg->header.size <= MAX_LED_NUMBER);
        for (int i = 0; i < msg->header.size; i++)
        {
            if ((int8_t)msg->data[i] != 0)
            {
                if (((int)matrix[i].b + (int8_t)msg->data[i]) >= 0)
                {
                    matrix[i].b += (int8_t)msg->data[i];
                    matrix[i].g += (int8_t)msg->data[i];
                    matrix[i].r += (int8_t)msg->data[i];
                }
                else // offset to zero
                {
                    matrix[i].b += 0;
                    matrix[i].g += 0;
                    matrix[i].r += 0;
                }
            }
        }
    }
}
