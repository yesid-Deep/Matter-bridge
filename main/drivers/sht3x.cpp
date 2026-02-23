/*
   Driver para SHT3x (Temperatura y Humedad) usando el NUEVO driver I2C (driver_ng)
*/

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "sht3x.h" // Asegúrate de tener el sht3x.h que creamos antes

static const char *TAG = "sht3x";

// Comandos I2C específicos para el SHT3x (High repeatability, clock stretching enabled)
#define SHT3X_CMD_MEASURE_MSB 0x2C
#define SHT3X_CMD_MEASURE_LSB 0x06

typedef struct {
    sht3x_sensor_config_t *config;
    esp_timer_handle_t timer;
    i2c_master_dev_handle_t i2c_dev; // Nuevo: Handle del dispositivo I2C
    bool is_initialized = false;
} sht3x_sensor_ctx_t;

static sht3x_sensor_ctx_t s_ctx;

// Función para enviar el comando de medición y leer los datos usando driver_ng
static esp_err_t sht3x_read(uint8_t *data, size_t size) {
    if (!s_ctx.i2c_dev) return ESP_ERR_INVALID_STATE;

    // Comando de 2 bytes para iniciar medición
    uint8_t cmd[2] = {SHT3X_CMD_MEASURE_MSB, SHT3X_CMD_MEASURE_LSB};
    
    // Con el nuevo driver, usamos i2c_master_transmit para escribir
    esp_err_t err = i2c_master_transmit(s_ctx.i2c_dev, cmd, sizeof(cmd), -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error enviando comando: %s", esp_err_to_name(err));
        return err;
    }

    // El SHT3x necesita unos 15ms para procesar la medida de alta precisión
    vTaskDelay(pdMS_TO_TICKS(20));

    // Con el nuevo driver, usamos i2c_master_receive para leer
    err = i2c_master_receive(s_ctx.i2c_dev, data, size, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error leyendo datos: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

// Convertir datos crudos a Temperatura (Fórmula del Datasheet SHT3x)
static float sht3x_get_temp(uint16_t raw_temp) {
    return -45.0f + (175.0f * (static_cast<float>(raw_temp) / 65535.0f));
}

// Convertir datos crudos a Humedad (Fórmula del Datasheet SHT3x)
static float sht3x_get_humidity(uint16_t raw_humidity) {
    return 100.0f * (static_cast<float>(raw_humidity) / 65535.0f);
}

static esp_err_t sht3x_get_read_temp_and_humidity(float &temp, float &humidity) {
    // 6 bytes: 2 temp + 1 CRC + 2 hum + 1 CRC
    uint8_t data[6] = {0};

    esp_err_t err = sht3x_read(data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    // Ignoramos el CRC (byte 2 y byte 5) por simplicidad
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humidity = (data[3] << 8) | data[4];

    temp = sht3x_get_temp(raw_temp);
    humidity = sht3x_get_humidity(raw_humidity);

    return ESP_OK;
}

static void timer_cb_internal(void *arg) {
    auto *ctx = (sht3x_sensor_ctx_t *) arg;
    if (!(ctx && ctx->config)) {
        return;
    }

    float temp, humidity;
    esp_err_t err = sht3x_get_read_temp_and_humidity(temp, humidity);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error leyendo sensor en el timer");
        return;
    }
    
    if (ctx->config->temperature.cb) {
        ctx->config->temperature.cb(ctx->config->temperature.endpoint_id, temp, ctx->config->user_data);
    }
    if (ctx->config->humidity.cb) {
        ctx->config->humidity.cb(ctx->config->humidity.endpoint_id, humidity, ctx->config->user_data);
    }
}

esp_err_t sht3x_sensor_init(sht3x_sensor_config_t *config) {
    if (config == NULL || config->i2c_bus == NULL) {
        ESP_LOGE(TAG, "Configuración inválida o bus I2C faltante");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_ctx.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config = config;

    // --- CONFIGURACIÓN DEL NUEVO DRIVER I2C ---
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = config->i2c_addr;
    dev_cfg.scl_speed_hz = 100000; // 100 kHz

    // Agregar el SHT3x al bus I2C que le pasamos
    esp_err_t err = i2c_master_bus_add_device(config->i2c_bus, &dev_cfg, &s_ctx.i2c_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al agregar dispositivo I2C: %d", err);
        return err;
    }

    // --- CONFIGURACIÓN DEL TIMER ---
    esp_timer_create_args_t args = {};
    args.callback = timer_cb_internal;
    args.arg = &s_ctx;
    
    err = esp_timer_create(&args, &s_ctx.timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed, err:%d", err);
        return err;
    }

    err = esp_timer_start_periodic(s_ctx.timer, config->interval_ms * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_periodic failed: %d", err);
        return err;
    }

    s_ctx.is_initialized = true;
    ESP_LOGI(TAG, "sht3x inicializado correctamente con driver_ng");

    return ESP_OK;
}