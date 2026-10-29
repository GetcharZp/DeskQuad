// memorysaver.h 选择本工程使用的相机模块
//
// 这是相对上游 ArduCAM 唯一的改动点：上游默认勾选 OV5642_MINI_5MP_PLUS，
// 会让库内所有 OV2640 分支（InitCAM 的 case OV2640、OV2640_set_JPEG_size、
// OV2640_set_Special_effects 等）被条件编译掉，传感器实际从未被配置。
// 本工程用 ArduCAM Mini 2MP Plus (OV2640)，这里必须选中对应宏。

#ifndef _MEMORYSAVER_
#define _MEMORYSAVER_

//Only when using raspberry,enable it
//#define RASPBERRY_PI

//Step 1: select the hardware platform, only one at a time
//#define OV2640_MINI_2MP
//#define OV3640_MINI_3MP
//#define OV5642_MINI_5MP
//#define OV5642_MINI_5MP_BIT_ROTATION_FIXED
#define OV2640_MINI_2MP_PLUS
//#define OV5642_MINI_5MP_PLUS
//#define OV5640_MINI_5MP_PLUS

#endif	//_MEMORYSAVER_
