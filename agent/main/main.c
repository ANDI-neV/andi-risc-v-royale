#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_http_client.h"
#include "esp_tls.h"

#include "config.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAGHTTP = "HTTP_CLIENT";



/** DEFINES **/
#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define TCP_SUCCESS 1 << 0
#define TCP_FAILURE 1 << 1
#define MAX_FAILURES 10


// GAME VARIABLES
char token[9] = "00000000"; // This is for debug reasons so we can set wrong tokens

// Struct for client thing

typedef struct {
    esp_http_client_handle_t client;
    char *response_buffer;
}client_thingy;

typedef struct {
    int length;
    int x;
    int y;
    int direction; // 0 = right, 1 = down
}ship;

char board[100]; // "W", "H"; Water and Hit

const char* ship_data(ship ship){
    char *data = malloc(100);
    memset(data, 0, 100);
    strcat(data, "{\"l\":");
    char length[2];
    sprintf(length, "%d", ship.length);
    strcat(data, length);
    strcat(data, ",\"x\":");
    char x[2];
    sprintf(x, "%d", ship.x);
    strcat(data, x);
    strcat(data, ",\"y\":");
    char y[2];
    sprintf(y, "%d", ship.y);
    strcat(data, y);
    strcat(data, ",\"d\":");
    char direction[2];
    sprintf(direction, "%d", ship.direction);
    strcat(data, direction);
    strcat(data, "}");
    return data;
}

/** GLOBALS **/

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static int s_retry_num = 0;

// task tag
static const char *TAG = "WIFI";
/** FUNCTIONS **/

//event handler for wifi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ESP_LOGI(TAG, "Connecting to AP...");
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < MAX_FAILURES)
		{
			ESP_LOGI(TAG, "Reconnecting to AP...");
			esp_wifi_connect();
			s_retry_num++;
		} else {
			xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
		}
	}
}

//event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }

}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAGHTTP, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAGHTTP, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAGHTTP, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAGHTTP, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAGHTTP, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAGHTTP, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

// connect to wifi and return the result
esp_err_t connect_wifi()
{
	int status = WIFI_FAILURE;

	/** INITIALIZE ALL THE THINGS **/
	//initialize the esp network interface
	ESP_ERROR_CHECK(esp_netif_init());

	//initialize default esp event loop
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	//create wifi station in the wifi driver
	esp_netif_create_default_wifi_sta();

	//setup wifi station with the default wifi configuration
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /** EVENT LOOP CRAZINESS **/
	wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &wifi_handler_event_instance));

    esp_event_handler_instance_t got_ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &got_ip_event_instance));

    /** START THE WIFI DRIVER **/
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA initialization complete");

    /** NOW WE WAIT **/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_SUCCESS | WIFI_FAILURE,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_SUCCESS) {
        ESP_LOGI(TAG, "Connected to ap");
        status = WIFI_SUCCESS;
    } else if (bits & WIFI_FAILURE) {
        ESP_LOGI(TAG, "Failed to connect to ap");
        status = WIFI_FAILURE;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}


static void http_rest_with_url(client_thingy thing)
{
    

    const char *post_data = "{\"field1\":\"value1\"}";
    
    esp_http_client_set_method(thing.client, HTTP_METHOD_POST);
    ESP_LOGI(TAGHTTP, "Attempting perform");
    esp_http_client_set_header(thing.client, "Content-Type", "application/json");
    esp_http_client_set_post_field(thing.client, post_data, strlen(post_data));
    esp_http_client_set_url(thing.client, "/activity");
    esp_err_t err = esp_http_client_perform(thing.client);
    if (err == ESP_OK) {
        ESP_LOGI(TAGHTTP, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(thing.client),
                esp_http_client_get_content_length(thing.client));
    } else {
        ESP_LOGE(TAGHTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    ESP_LOG_BUFFER_CHAR(TAGHTTP, thing.response_buffer, strlen(thing.response_buffer));

    
    esp_http_client_cleanup(thing.client);
}

static void register_player(client_thingy thing){
    char *post_data = malloc(30);
    memset(post_data, 0, 30);
    strcat(post_data, "{\"name\":\"");
    strcat(post_data, AGENTNAME);
    strcat(post_data, "\"}");
    esp_http_client_set_method(thing.client, HTTP_METHOD_POST);
    esp_http_client_set_header(thing.client, "Content-Type", "application/json");
    esp_http_client_set_post_field(thing.client, post_data, strlen(post_data));
    esp_http_client_set_url(thing.client, "/register");
    esp_err_t err = esp_http_client_perform(thing.client);
    if (err == ESP_OK) {
        ESP_LOGI(TAGHTTP, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(thing.client),
                esp_http_client_get_content_length(thing.client));
    } else {
        ESP_LOGE(TAGHTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    strncpy(token, thing.response_buffer + 14, 8);
    free(post_data);
    esp_http_client_cleanup(thing.client);
}

static void start_game(client_thingy thing, ship *ships){
    char *post_data = malloc(300);
    memset(post_data, 0, 300);
    strcat(post_data, "{\"name\":\"");
    strcat(post_data, AGENTNAME);
    strcat(post_data,"\", \"token\":\"");
    strcat(post_data, token);
    strcat(post_data, "\", \"carrier\":");
    strcat(post_data, ship_data(ships[0]));
    strcat(post_data, ", \"battleship\":");
    strcat(post_data, ship_data(ships[1]));
    strcat(post_data, ", \"cruiser\":");
    strcat(post_data, ship_data(ships[2]));
    strcat(post_data, ", \"submarine\":");
    strcat(post_data, ship_data(ships[3]));
    strcat(post_data, ", \"destroyer\":");
    strcat(post_data, ship_data(ships[4]));
    strcat(post_data, "}");

    esp_http_client_set_method(thing.client, HTTP_METHOD_POST);
    esp_http_client_set_header(thing.client, "Content-Type", "application/json");
    esp_http_client_set_post_field(thing.client, post_data, strlen(post_data));
    esp_http_client_set_url(thing.client, "/start");
    esp_err_t err = esp_http_client_perform(thing.client);
    if (err == ESP_OK) {
        ESP_LOGI(TAGHTTP, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(thing.client),
                esp_http_client_get_content_length(thing.client));
    } else {
        ESP_LOGE(TAGHTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    free(post_data);
    esp_http_client_cleanup(thing.client);
}

// 0 = fail, 1 = idle, 2 = wait, 3 = play, 4 = turn
int get_state(client_thingy thing){
    char *post_data = malloc(50);
    memset(post_data, 0, 50);
    strcat(post_data, "{\"name\":\"");
    strcat(post_data, AGENTNAME);
    strcat(post_data,"\", \"token\":\"");
    strcat(post_data, token);
    strcat(post_data, "\"}");
    esp_http_client_set_method(thing.client, HTTP_METHOD_POST);
    esp_http_client_set_header(thing.client, "Content-Type", "application/json");
    esp_http_client_set_post_field(thing.client, post_data, strlen(post_data));
    esp_http_client_set_url(thing.client, "/state");
    esp_err_t err = esp_http_client_perform(thing.client);
    if (err == ESP_OK) {
        ESP_LOGI(TAGHTTP, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(thing.client),
                esp_http_client_get_content_length(thing.client));
    } else {
        ESP_LOGE(TAGHTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    char state[5];
    strncpy(state, thing.response_buffer + 14, 4);
    free(post_data);
    esp_http_client_cleanup(thing.client);
    switch(state[0]){
        case 'f':
            return 0;
        case 'i':
            return 1;
        case 'w':
            return 2;
        case 'p':
            return 3;
        case 't':
            return 4;
        default:
            return 0;
    }
}

static void get_board(client_thingy thing){
    char *post_data = malloc(50);
    memset(post_data, 0, 50);
    strcat(post_data, "{\"name\":\"");
    strcat(post_data, AGENTNAME);
    strcat(post_data,"\", \"token\":\"");
    strcat(post_data, token);
    strcat(post_data, "\"}");
    esp_http_client_set_method(thing.client, HTTP_METHOD_POST);
    esp_http_client_set_header(thing.client, "Content-Type", "application/json");
    esp_http_client_set_post_field(thing.client, post_data, strlen(post_data));
    esp_http_client_set_url(thing.client, "/board");
    esp_err_t err = esp_http_client_perform(thing.client);
    if (err == ESP_OK) {
        ESP_LOGI(TAGHTTP, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(thing.client),
                esp_http_client_get_content_length(thing.client));
    } else {
        ESP_LOGE(TAGHTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    free(post_data);
    ESP_LOG_BUFFER_CHAR(TAGHTTP, thing.response_buffer, strlen(thing.response_buffer));
    esp_http_client_cleanup(thing.client);
}

client_thingy* create_http_client()
{
    client_thingy* thing = malloc(sizeof(client_thingy));
    if (thing == NULL) {
        return NULL;
    }
    thing->response_buffer = malloc(MAX_HTTP_OUTPUT_BUFFER + 1);
    if (thing->response_buffer == NULL) {
        free(thing);
        return NULL;
    }
    memset(thing->response_buffer, 0, MAX_HTTP_OUTPUT_BUFFER + 1); // Ich fülle den Buffer mit Nullen

    esp_http_client_config_t config = {
        .host = CONFIG_HTTP_ENDPOINT,
        .path = "/activity",
        .query = "esp",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .port = 11449,
        .event_handler = _http_event_handler,
        .user_data = thing->response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = true,
    };
    thing->client = esp_http_client_init(&config);
    return thing;
}


void app_main(void)
{
    
	esp_err_t status = WIFI_FAILURE;

	//initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // connect to wireless AP
	status = connect_wifi();
	if (WIFI_SUCCESS != status)
	{
		ESP_LOGI(TAG, "Failed to associate to AP, dying...");
		return;
	}
    client_thingy *thing = malloc(sizeof(client_thingy));
    thing = create_http_client();
    get_state(*thing);
    thing = create_http_client();
    register_player(*thing);
    thing = create_http_client();
    ship carrier = {5, 0, 0, 0};
    ship battleship = {4, 1, 0, 0};
    ship cruiser = {3, 2, 0, 0};
    ship submarine = {3, 3, 0, 0};
    ship destroyer = {2, 4, 0, 0};
    ship ships[5] = {carrier, battleship, cruiser, submarine, destroyer};
    start_game(*thing, ships);
    while(1){
        while (1){
            thing = create_http_client();
            int state = get_state(*thing);
            if (state == 4){
                break;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAGHTTP, "It's my turn");
        // Get board
        thing = create_http_client();
        get_board(*thing);
    }
    

}