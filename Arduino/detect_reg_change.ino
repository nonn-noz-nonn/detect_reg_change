int ADIN_PIN = 0;
int LED_PIN = 2;

typedef enum STATE_T {
    STOP = 0,
    RUN,
} STATE;

typedef struct PARAM_T {
    uint16_t th_btm;
    uint16_t th_top;
    uint16_t th_led;
    uint8_t led_logic;
    float lpf_a0;
    unsigned long delay_ms;
} PARAM;

const int COMMAND_QUIT = 'q';
const int COMMAND_START = 's';
const int COMMAND_READ = 'r';
const int COMMAND_WRITE = 'w';

void recv_command(STATE *sts, PARAM *prm);
void send_param(const PARAM *prm);
void recv_param(PARAM *prm);
bool delimit_str_recv(String str, char token[][16]);
void parse_token(const char token[][16], PARAM *prm);
void send_result(const PARAM *prm);

void setup() {
    // put your setup code here, to run once:
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(9600);
    Serial.setTimeout(100);
}

void loop() {
    // put your main code here, to run repeatedly:
    static STATE state = STOP;
    static PARAM param = {0U, 1023U, 511U, 0U, 0.5f, 10UL};
    
    if ( Serial.available() > 0 ) {
        recv_command(&state, &param);
    }
    
    if ( RUN == state ) {
        send_result(&param);
    }
}

void recv_command(STATE *sts, PARAM *prm)
{
    int recv = Serial.read();
    if ( COMMAND_QUIT == recv ) {
        if ( RUN == *sts ) {
            digitalWrite(LED_PIN, LOW);
            *sts = STOP;
        }
    } else if ( COMMAND_START == recv ) {
        if ( STOP == *sts ) {
            *sts = RUN;
        }
    } else if ( COMMAND_READ == recv ) {
        if ( STOP == *sts ) {
            send_param(prm);
        }
    } else if ( COMMAND_WRITE == recv ) {
        if ( STOP == *sts ) {
            recv_param(prm);
        }
    }
    return;
}

void send_param(const PARAM *prm)
{
    String str_th_btm = String(prm->th_btm, DEC);
    String str_th_top = String(prm->th_top, DEC);
    String str_th_led = String(prm->th_led, DEC);
    String str_led_logic = String(prm->led_logic, DEC);
    String str_lpf_a0 = String(prm->lpf_a0, 3);
    String str_delay_ms = String(prm->delay_ms, DEC);
    String str_resp = String(str_th_btm + "," + 
                             str_th_top + "," + 
                             str_th_led + "," + 
                             str_led_logic + "," + 
                             str_lpf_a0 + "," + 
                             str_delay_ms);
    Serial.println(str_resp);
    return;
}

void recv_param(PARAM *prm)
{
    String str_recv = "";
    while ( 0 == str_recv.length() ) {
        str_recv = Serial.readString();
        str_recv.trim();
    }
    
    str_recv.replace(" ", "");
    
    char token[6][16];
    memset(token, '\0', sizeof(token));
    
    bool ret = delimit_str_recv(str_recv, token);
    if ( false == ret ) {
        goto EXIT;
    }
    
    parse_token(token, prm);
    
EXIT:
    return;
}

bool delimit_str_recv(String str, char token[][16])
{
    bool ret = false;
    
    char *str_ptr = (char *)(str.c_str()); 
    char *delim_ptr = NULL;
    uint8_t token_cnt = 0U;
    
    delim_ptr = strtok(str_ptr, ",");
    snprintf(token[token_cnt], 16, "%s", delim_ptr);
    token_cnt++;
    
    while ( NULL != delim_ptr && token_cnt < 6 ) {
        delim_ptr = strtok(NULL, ",");
        if ( NULL != delim_ptr ) {
            snprintf(token[token_cnt], 16, "%s", delim_ptr);
            token_cnt++;
        }
    }
    
    if ( 6 != token_cnt ) {
        Serial.println("E00: number of param invalid.");
        ret = false;
        goto EXIT;
    }
    ret = true;
    
EXIT:
    return ret;
}

void parse_token(const char token[][16], PARAM *prm)
{
    PARAM tmp_prm;
    memset(&tmp_prm, 0, sizeof(tmp_prm));
    
    unsigned long num = 0U;
    char *endptr = NULL;
    float fnum = 0.0f;
    
    num = strtoul(token[0], &endptr, 10);
    if ( '\0' != *endptr || num > 1023U ) {
        Serial.println("E01: threshold of bottom [0-1023] invalid.");
        goto EXIT;
    }
    tmp_prm.th_btm = (uint16_t)num;
    
    num = strtoul(token[1], &endptr, 10);
    if ( '\0' != *endptr || num > 1023U ) {
        Serial.println("E02: threshold of top [0-1023] invalid.");
        goto EXIT;
    }
    tmp_prm.th_top = (uint16_t)num;
    
    num = strtoul(token[2], &endptr, 10);
    if ( '\0' != *endptr || num > 1023U ) {
        Serial.println("E03: threshold of LED [0-1023] invalid.");
        goto EXIT;
    }
    tmp_prm.th_led = (uint16_t)num;
    
    num = strtoul(token[3], &endptr, 10);
    if ( '\0' != *endptr || num > 1U ) {
        Serial.println("E04: logic of LED [0-1] invalid.");
        goto EXIT;
    }
    tmp_prm.led_logic = (uint8_t)num;
    
    fnum = (float)strtod(token[4], &endptr);
    if ( '\0' != *endptr || fnum > 1.0f || fnum < 0.0f ) {
        Serial.println("E05: a0 of LPF [0.0-1.0] invalid.");
        goto EXIT;
    }
    tmp_prm.lpf_a0 = fnum;
    
    num = strtoul(token[5], &endptr, 10);
    if ( '\0' != *endptr ) {
        Serial.println("E06: delay time [msec] invalid.");
        goto EXIT;
    }
    tmp_prm.delay_ms = num;
    
    if ( tmp_prm.th_top <= tmp_prm.th_btm ) {
        Serial.println("E07: threshold of top must be larger than of bottom.");
        goto EXIT;
    }
    
    if ( tmp_prm.th_led < tmp_prm.th_btm || tmp_prm.th_led > tmp_prm.th_top ) {
        Serial.println("E08: threshold of led must be a value between of bottom and of top.");
        goto EXIT;
    }
    
    memcpy(prm, &tmp_prm, sizeof(tmp_prm));
EXIT:
    return;
}

void send_result(const PARAM *prm)
{
    uint16_t adc_val = analogRead(ADIN_PIN);
    
    static float y[2] = {0};
    y[1] = prm->lpf_a0 * y[0] + (1.0 - prm->lpf_a0) * adc_val;
    y[0] = y[1];
    uint16_t dout = (uint16_t)y[1];
    
    if ( dout < prm->th_btm ) {
        dout = prm->th_btm;
    } else if ( dout > prm->th_top ) {
        dout = prm->th_top;
    }
    
    if ( dout < prm->th_led ) {
        if ( 0U == prm->led_logic ) {
            digitalWrite(LED_PIN, HIGH);
        } else {
            digitalWrite(LED_PIN, LOW);
        }
    } else {
        if ( 0U == prm->led_logic ) {
            digitalWrite(LED_PIN, LOW);
        } else {
            digitalWrite(LED_PIN, HIGH);
        }
    }
    
    Serial.println(dout, DEC);
    
    delay(prm->delay_ms);
}
