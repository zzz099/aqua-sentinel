#ifndef __AQUARIUM_LIGHT_H__
#define __AQUARIUM_LIGHT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/*
 * 水族灯/照明继电器控制引脚。
 * 如果后续硬件接到了其他 GPIO，只需要修改下面 3 个宏。
 */
#define AQUARIUM_LIGHT_PORT         GPIOC
#define AQUARIUM_LIGHT_PIN          GPIO_PIN_1
#define AQUARIUM_LIGHT_ACTIVE_STATE GPIO_PIN_SET

void aquarium_light_init(void);
void aquarium_light_on(void);
void aquarium_light_off(void);
bool aquarium_light_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __AQUARIUM_LIGHT_H__ */
