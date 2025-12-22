#pragma once

#include "lvgl.h"

void checkNatsConnection(lv_timer_t *timer);
void connectNats();
void loopNats();
void setupNats();

void publishLogMessage(const char* message);
