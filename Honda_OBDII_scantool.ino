#include <WiFi.h>
#include <WebServer.h>
#include "driver/twai.h"

//WiFi settings for webpage
const char* ssid = "CRV_Diagnostic_Scanner";
const char* password = "hondapassword";
WebServer server(80);

#define CAN_TX_PIN GPIO_NUM_6
#define CAN_RX_PIN GPIO_NUM_7


//Honda PIDs for Powertrain Control Unit's data tables
uint16_t active_pids[] = {0x3004, 0x2301};
int current_pid_idx = 0;

uint8_t payload_buf[255];
int payload_len = 0;
int payload_idx = 0;
bool receiving_multi = false;

//metrics to display on webpage
float engine_rpm = 0.0;
float dash_speed = 0.0;
float coolant_temp = 0.0;
float cvt_temp = 0.0;
int throttle_load = 0;
int primary_pulley = 0;
int secondary_pulley = 0;
float cvt_ratio = 0.0;

unsigned long lastHeartbeat = 0;
unsigned long lastQuery = 0;
bool sessionStarted = false;

//html for webpage
const char* htmlPage PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CR-V Master Dashboard</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #050a11; color: #ffffff; text-align: center; padding: 10px; margin: 0; }
    h1 { color: #00bcd4; text-transform: uppercase; letter-spacing: 2px; font-size: 1.4em; margin-bottom: 20px; margin-top: 10px;}
    .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; max-width: 600px; margin: 0 auto; padding: 0 10px; }
    
    .card { background-color: #0b1320; border-radius: 12px; padding: 15px 5px; border-top: 4px solid #00bcd4; box-shadow: 0 6px 12px rgba(0,0,0,0.6); display: flex; flex-direction: column; justify-content: center; transition: border-color 0.3s; }
    .card.hot { border-top: 4px solid #ff5252; }
    .card.cvt { border-top: 4px solid #ffeb3b; }
    .card.pulley { border-top: 4px solid #00ff66; }
    .card.ratio { border-top: 5px solid #bc7afb; grid-column: span 2; padding: 20px 10px; background-color: #111a2e;}
    
    .value { font-size: 2.2em; font-weight: bold; margin-top: 5px; text-shadow: 1px 1px 2px #000; }
    .label { font-size: 0.75em; color: #8da0c2; text-transform: uppercase; letter-spacing: 1px; }
    .unit { font-size: 0.5em; color: #6b7280; font-weight: normal; }
  </style>
</head>
<body>
  <h1>Honda Earth Dreams Monitor</h1>
  
  <div class="grid">
    <div class="card">
      <div class="label">Engine Speed</div>
      <div class="value" id="rpm">--</div>
    </div>
    <div class="card pulley">
      <div class="label">Primary Pulley</div>
      <div class="value" id="pri" style="color: #00ff66;">--</div>
    </div>

    <div class="card" id="slipCard">
      <div class="label" id="slipLabel">TCC Slip</div>
      <div class="value" id="slip">--</div>
    </div>
    <div class="card">
      <div class="label">Vehicle Speed</div>
      <div class="value" id="speed">-- <span class="unit">km/h</span></div>
    </div>
    
    <div class="card hot">
      <div class="label">Coolant Temp</div>
      <div class="value" id="coolant" style="color: #ff5252;">-- <span class="unit">&deg;C</span></div>
    </div>
    <div class="card cvt">
      <div class="label">CVT Fluid Temp</div>
      <div class="value" id="cvt_t" style="color: #ffeb3b;">-- <span class="unit">&deg;C</span></div>
    </div>

    <div class="card ratio">
      <div class="label" style="color: #bc7afb; font-size: 1.1em;">Active CVT Gear Ratio</div>
      <div class="value" id="ratio" style="color: #bc7afb; font-size: 3.5em; margin-top: 10px;">--</div>
      <div class="label" style="margin-top: 15px; font-size: 0.8em; color: #fff;" id="load">Engine Load: -- %</div>
    </div>
  </div>

  <script>
    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        document.getElementById('rpm').innerHTML = data.rpm.toFixed(0) + ' <span class="unit">RPM</span>';
        document.getElementById('pri').innerHTML = data.pri + ' <span class="unit">RPM</span>';
        document.getElementById('speed').innerHTML = data.dash_speed.toFixed(0) + ' <span class="unit">km/h</span>';
        document.getElementById('coolant').innerHTML = data.coolant.toFixed(0) + ' <span class="unit">&deg;C</span>';
        document.getElementById('cvt_t').innerHTML = data.cvt_t.toFixed(0) + ' <span class="unit">&deg;C</span>';
        document.getElementById('load').innerText = "Engine Load: " + data.load + " %";
        
        // Torque Converter Slip Logic
        let slipCard = document.getElementById('slipCard');
        let slipLabel = document.getElementById('slipLabel');
        let slipValue = document.getElementById('slip');
        
        if (data.rpm > 0) {
            let slipAmt = data.slip;
            slipValue.innerHTML = Math.abs(slipAmt) + ' <span class="unit">RPM</span>';
            
            // If slip is less than 30 RPM, it's mechanically locked
            if (Math.abs(slipAmt) < 30) {
                slipCard.style.borderTop = "4px solid #00ff66";
                slipValue.style.color = "#00ff66";
                slipLabel.innerText = "TCC: LOCKED";
            } else {
                slipCard.style.borderTop = "4px solid #ffeb3b";
                slipValue.style.color = "#ffeb3b";
                slipLabel.innerText = "TCC: SLIPPING";
            }
        }

        if (data.ratio > 0 && data.sec > 0) {
            document.getElementById('ratio').innerText = data.ratio.toFixed(3);
        } else {
            document.getElementById('ratio').innerText = "2.630"; 
        }
      });
    }, 150);
  </script>
</body>
</html>
)=====";

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  
  //Configure and start webpage
  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  
  server.on("/data", []() {
    String json = "{";
    json += "\"rpm\":" + String(engine_rpm) + ",";
    json += "\"dash_speed\":" + String(dash_speed) + ",";
    json += "\"coolant\":" + String(coolant_temp) + ",";
    json += "\"cvt_t\":" + String(cvt_temp) + ",";
    json += "\"load\":" + String(throttle_load) + ",";
    json += "\"pri\":" + String(primary_pulley) + ",";
    json += "\"sec\":" + String(secondary_pulley) + ",";
    json += "\"slip\":" + String((int)engine_rpm - primary_pulley) + ",";
    json += "\"ratio\":" + String(cvt_ratio, 3);
    json += "}";
    server.send(200, "application/json", json);
  });
  
  server.begin();

  //Two wire automotive interface configuration
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  g_config.rx_queue_len = 256; 
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();
}


void sendCanFrame(uint32_t id, bool isExtended, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7) {
  twai_message_t message;
  memset(&message, 0, sizeof(twai_message_t)); 
  message.identifier = id;
  message.extd = isExtended ? 1 : 0; 
  message.data_length_code = 8;
  message.data[0] = d0; message.data[1] = d1; message.data[2] = d2; message.data[3] = d3;
  message.data[4] = d4; message.data[5] = d5; message.data[6] = d6; message.data[7] = d7;
  twai_transmit(&message, pdMS_TO_TICKS(5));
}

void loop() {
  server.handleClient(); 

  twai_message_t rx_msg;
  while (twai_receive(&rx_msg, 0) == ESP_OK) {
    
    //passive CAN sniffing
    if (rx_msg.identifier == 0x17C) {
      float rpm = (rx_msg.data[2] * 256.0) + rx_msg.data[3];
      if (rpm >= 0 && rpm < 8000) engine_rpm = rpm;
    }
    else if (rx_msg.identifier == 0x158) {
      float raw_speed = (rx_msg.data[0] * 256.0) + rx_msg.data[1];
      dash_speed = (raw_speed * 0.01 >= 0 && raw_speed * 0.01 < 240) ? raw_speed * 0.01 : 0;
    }
    

    else if (rx_msg.identifier == 0x18DAF10E) {
      uint8_t frame_type = rx_msg.data[0] & 0xF0;


      if (frame_type == 0x10) {
        payload_len = (((rx_msg.data[0] & 0x0F) << 8) | rx_msg.data[1]); 
        payload_idx = 0;
        memset(payload_buf, 0, sizeof(payload_buf));
        receiving_multi = true;
        
        for (int i = 2; i <= 7; i++) {
          payload_buf[payload_idx++] = rx_msg.data[i];
        }
        sendCanFrame(0x18DA0EF1, true, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); 
      }


      else if (frame_type == 0x20 && receiving_multi) {
        for (int i = 1; i <= 7; i++) {
          if (payload_idx < payload_len && payload_idx < 255) {
            payload_buf[payload_idx++] = rx_msg.data[i];
          }
        }
        
        if (payload_idx >= payload_len) {
          receiving_multi = false;
          
          uint16_t completed_pid = (payload_buf[1] << 8) | payload_buf[2];
          
          if (completed_pid == 0x3004) {
            coolant_temp  = payload_buf[10] - 40.0;
            cvt_temp      = payload_buf[12] - 40.0;
            throttle_load = payload_buf[27]; 
          }
          else if (completed_pid == 0x2301) {
            primary_pulley   = (payload_buf[12] << 8) | payload_buf[13];
            secondary_pulley = (payload_buf[14] << 8) | payload_buf[15];
            
            if (secondary_pulley > 0) {
                cvt_ratio = (float)primary_pulley / (float)secondary_pulley;
            } else {
                cvt_ratio = 0.0;
            }
          }
        }
      }
    }
  }

  //active CAN requests
  unsigned long currentMillis = millis();


  if (!sessionStarted && currentMillis > 3000) {
    sendCanFrame(0x18DA0EF1, true, 0x02, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00);
    sessionStarted = true;
    lastHeartbeat = currentMillis;
  }


  if (sessionStarted && (currentMillis - lastHeartbeat >= 2000)) {
    lastHeartbeat = currentMillis;
    sendCanFrame(0x18DA0EF1, true, 0x02, 0x3E, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00); 
  }


  if (sessionStarted && (currentMillis - lastQuery >= 100)) { 
    lastQuery = currentMillis;
    
    uint16_t target_pid = active_pids[current_pid_idx];
    sendCanFrame(0x18DA0EF1, true, 0x03, 0x22, (target_pid >> 8) & 0xFF, target_pid & 0xFF, 0x00, 0x00, 0x00, 0x00);
    
    current_pid_idx++;
    if (current_pid_idx >= 2) {
      current_pid_idx = 0;
    }
  }
}