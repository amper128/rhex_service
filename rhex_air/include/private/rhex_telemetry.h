/**
 * @file rhex_telemetry.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020-2021
 * @brief Работа с телеметрией
 */

#pragma once

#include <svc/platform.h>
#include <proto/telemetry.h>

int rhex_telemetry_init(void);

int rhex_telemetry_main(void);
