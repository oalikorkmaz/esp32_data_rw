#ifndef SERIAL_IF_H
#define SERIAL_IF_H

#include <stdbool.h>

void serial_init(void); //UART’ı yapılandırıyor (TX=17, RX=16, 9600 baud)
void serial_start(void); // Görevi başlatıyor (FreeRTOS task olarak)

#endif
