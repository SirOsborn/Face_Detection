// camera.h - Camera module interface
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t size;
} image_t;

void camera_init();
image_t camera_capture();

#endif // CAMERA_H
