/*
   Driver para SHT3x (Temperatura y Humedad)
   Implementado para requerimientos de Matter.
*/

#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
// Incluimos el NUEVO driver I2C de ESP-IDF v5.2+
#include "driver/i2c_master.h" 

using sht3x_sensor_cb_t = void (*)(uint16_t endpoint_id, float value, void *user_data);

typedef struct {
    struct {
        // Callback para reportar temperatura
        sht3x_sensor_cb_t cb = NULL;
        // Endpoint ID asociado al sensor de temperatura (Matter)
        uint16_t endpoint_id;
    } temperature;

    struct {
        // Callback para reportar humedad
        sht3x_sensor_cb_t cb = NULL;
        // Endpoint ID asociado al sensor de humedad (Matter)
        uint16_t endpoint_id;
    } humidity;

    // Puntero al bus I2C ya inicializado (Nuevo Driver)
    i2c_master_bus_handle_t i2c_bus = NULL; 

    // Dirección I2C del sensor (SHT3x suele ser 0x44 o 0x45)
    uint8_t i2c_addr = 0x44; 

    // Datos del usuario
    void *user_data = NULL;

    // Intervalo de lectura en milisegundos, por defecto 5000 ms
    uint32_t interval_ms = 5000;
} sht3x_sensor_config_t;

/**
 * @brief Inicializa el driver del SHT3x y crea la tarea de lectura.
 *
 * @param config Configuraciones del sensor.
 *
 * @return esp_err_t - ESP_OK si es exitoso.
 */
esp_err_t sht3x_sensor_init(sht3x_sensor_config_t *config);