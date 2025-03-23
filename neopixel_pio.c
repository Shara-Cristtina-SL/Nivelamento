#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/adc.h" // Incluir biblioteca ADC
#include "ws2818b.pio.h"

// Definições de pinos (permanecem as mesmas)
#define LED_COUNT 25
#define LED_PIN 7
#define JOYSTICK_VRX 27
#define JOYSTICK_VRY 26
#define JOYSTICK_SW 22
#define BUTTON_COR_1 5
#define BUTTON_COR_2 6
#define BUZZER_ACERTO 10
#define BUZZER_ERRO 21
#define LED_ERRO 13
#define LED_ACERTO 11

// Definições de cores e direções (permanecem as mesmas)
#define COR_1_R 255
#define COR_1_G 0
#define COR_1_B 0
#define COR_2_R 0
#define COR_2_G 255
#define COR_2_B 0
#define COR_OFF_R 0
#define COR_OFF_G 0
#define COR_OFF_B 0
#define COR_PROGRESSO_R 0
#define COR_PROGRESSO_G 0
#define COR_PROGRESSO_B 255 // Azul para a barra de progresso

typedef enum {
    CIMA,
    BAIXO,
    ESQUERDA,
    DIREITA,
    CENTRO
} Direcao;

// Estrutura de pixel e protótipos de funções (permanecem as mesmas)
struct pixel_t {
  uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;

npLED_t leds[LED_COUNT];
PIO np_pio;
uint sm;

void npInit(uint pin);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();
int getIndex(int x, int y);
Direcao lerJoystick();
bool lerBotaoCor1();
bool lerBotaoCor2();
bool lerBotaoJoystick(); //Nova função para o botão do joystick
void tocarBuzzerAcerto(int duracao_ms);
void tocarBuzzerErro(int duracao_ms);
void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho);
bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho);
void desenharSeta(Direcao direcao, bool cor1, bool cor2);
void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b);
void acenderLedAcerto(int duracao_ms);
void acenderLedErro(int duracao_ms);
void mostrarBarraProgresso(int progresso); // Nova função para mostrar a barra

// Funções de inicialização do PIO e manipulação da matriz (permanecem as mesmas)
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24-(y * 5 + x);
    } else {
        return 24-(y * 5 + (4 - x));
    }
}

// Funções de leitura do joystick e botões (MODIFICADA)
Direcao lerJoystick() {
    const int16_t centro = 2048; // Valor central do ADC (ajuste se necessário)
    const int16_t deadzone = 200;  // Tamanho da "zona morta" em torno do centro
    int16_t vrx_value = adc_read(); // Ler valor ADC do pino VRX
    adc_select_input(0);  // Garante que o ADC esteja lendo o pino correto
    int16_t vry_value = adc_read(); // Ler valor ADC do pino VRY
    adc_select_input(1); //Garante que o ADC está lendo o pino correto

    //Ajuste os limites conforme necessário com base nas leituras reais do seu joystick
    if (vrx_value < (centro - deadzone)) {
        return ESQUERDA;
    } else if (vrx_value > (centro + deadzone)) {
        return DIREITA;
    } else if (vry_value > (centro + deadzone)) {
        return CIMA;
    } else if (vry_value < (centro - deadzone)) {
        return BAIXO;
    } else {
        return CENTRO;
    }
}

bool lerBotaoCor1() {
    return !gpio_get(BUTTON_COR_1);
}

bool lerBotaoCor2() {
    return !gpio_get(BUTTON_COR_2);
}
bool lerBotaoJoystick() {
    return !gpio_get(JOYSTICK_SW);
}

// Funções de feedback (permanecem as mesmas)
void tocarBuzzerAcerto(int duracao_ms) {
    gpio_put(BUZZER_ACERTO, 1);
    sleep_ms(duracao_ms/2);
    gpio_put(BUZZER_ACERTO, 0);
    sleep_ms(duracao_ms/2);
}

void tocarBuzzerErro(int duracao_ms) {
    gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
    gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
    gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
    gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
}

void acenderLedAcerto(int duracao_ms) {
    gpio_put(LED_ACERTO, 1);
    sleep_ms(duracao_ms);
    gpio_put(LED_ACERTO, 0);
}

void acenderLedErro(int duracao_ms) {
    gpio_put(LED_ERRO, 1);
    sleep_ms(duracao_ms);
    gpio_put(LED_ERRO, 0);
}

// Funções de manipulação da matriz (permanecem as mesmas)
void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b){
    npClear();
    switch (direcao) {
        case CIMA:
            npSetLED(getIndex(2, 0), r, g, b);
            npSetLED(getIndex(1, 1), r, g, b);
            npSetLED(getIndex(3, 1), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(2, 4), r, g, b);
            break;
        case BAIXO:
            npSetLED(getIndex(2, 4), r, g, b);
            npSetLED(getIndex(1, 3), r, g, b);
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);
            npSetLED(getIndex(2, 0), r, g, b);
            break;
        case ESQUERDA:
            npSetLED(getIndex(0, 2), r, g, b);
            npSetLED(getIndex(1, 1), r, g, b);
            npSetLED(getIndex(1, 3), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(4, 2), r, g, b);
            break;
        case DIREITA:
            npSetLED(getIndex(4, 2), r, g, b);
            npSetLED(getIndex(3, 1), r, g, b);
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);
            npSetLED(getIndex(2, 2), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);
            npSetLED(getIndex(0, 2), r, g, b);
            break;
        default:
            npClear();
            break;
    }
    npWrite();
}

void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho) {
    for (int i = 0; i < tamanho; i++) {
        if (cores[i]) {
            mapearDirecaoNaMatriz(sequencia[i], COR_1_R, COR_1_G, COR_1_B);
        } else {
            mapearDirecaoNaMatriz(sequencia[i], COR_2_R, COR_2_G, COR_2_B);
        }
        sleep_ms(500);
        npClear();
        npWrite();
        sleep_ms(250);
    }
}

// Nova função para mostrar a barra de progresso
void mostrarBarraProgresso(int progresso) {
    // progresso: 0 a 100 (percentual)
    npClear();
    int num_leds_acesos = (int)((float)progresso / 100.0 * LED_COUNT);

    for (int i = 0; i < num_leds_acesos; i++) {
        npSetLED(i, COR_PROGRESSO_R, COR_PROGRESSO_G, COR_PROGRESSO_B);
    }
    npWrite();
}
bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho) {
    printf("Prepare-se! \n");
    sleep_ms(2000);
    printf("Start\n");

    for (int i = 0; i < tamanho; i++) {
        Direcao input_direcao = CENTRO;
        bool input_cor = false;
        bool botao_pressionado = false;
        int tempo_limite = 5000; // Tempo limite para cada entrada (5 segundos)
        int tempo_inicio = to_ms_since_boot(get_absolute_time());
        int tempo_decorrido;
        int progresso;

        // 1. Ler Direção do Joystick
        printf("Aguardando entrada do joystick para a direção %d...\n", i + 1);
        while (input_direcao == CENTRO && (tempo_decorrido = to_ms_since_boot(get_absolute_time()) - tempo_inicio) < tempo_limite) {
            Direcao direcao_lida = lerJoystick(); // Leitura temporária
            if (direcao_lida != CENTRO) {
                input_direcao = direcao_lida; // Aceita a leitura se for válida
            }
            sleep_ms(50);

            // Calcula e mostra o progresso
            progresso = (int)((float)tempo_decorrido / tempo_limite * 100);
            mostrarBarraProgresso(progresso);
        }

        if (input_direcao == CENTRO) { // Tempo esgotado
            printf("Tempo esgotado para a direção!\n");
            mostrarBarraProgresso(0); // Limpa a barra
            return false;
        }
        printf("Direção inserida: %d, Direção correta: %d\n", input_direcao, sequencia_correta[i]); // Debug

        // 2. Ler Cor (Botão)
        printf("Aguardando entrada do botão para a cor %d...\n", i + 1);
        tempo_inicio = to_ms_since_boot(get_absolute_time()); // Reseta o tempo para a cor
        while (!botao_pressionado && (tempo_decorrido = to_ms_since_boot(get_absolute_time()) - tempo_inicio) < tempo_limite) {
            if (lerBotaoCor1()) {
                input_cor = true;
                botao_pressionado = true;
            } else if (lerBotaoCor2()) {
                input_cor = false;
                botao_pressionado = true;
            }
            sleep_ms(50);

             // Calcula e mostra o progresso
            progresso = (int)((float)tempo_decorrido / tempo_limite * 100);
            mostrarBarraProgresso(progresso);
        }

        if (!botao_pressionado) { // Tempo esgotado
            printf("Tempo esgotado para a cor!\n");
            mostrarBarraProgresso(0); // Limpa a barra
            return false;
        }

        printf("Cor inserida: %s, Cor correta: %s\n", input_cor ? "Vermelho" : "Verde", cores_corretas[i] ? "Vermelho" : "Verde"); // Debug

        // Verifica se a direção está correta
        if (input_direcao != sequencia_correta[i]) {
            printf("Direcao incorreta! Esperado: %d, Recebido: %d\n", sequencia_correta[i], input_direcao);
            mostrarBarraProgresso(0); // Limpa a barra
            return false;
        }

        // Verifica se a cor está correta
        if (input_cor != cores_corretas[i]) {
            printf("Cor incorreta! Esperado: %s, Recebido: %s\n", cores_corretas[i] ? "Vermelho" : "Verde", input_cor ? "Vermelho" : "Verde");
            mostrarBarraProgresso(0); // Limpa a barra
            return false;
        }
         mostrarBarraProgresso(0); // Limpa a barra
    }

    return true;
}

int main() {
    stdio_init_all();
    gpio_init(JOYSTICK_VRX);
    gpio_init(JOYSTICK_VRY);
    gpio_init(JOYSTICK_SW);
    gpio_init(BUTTON_COR_1);
    gpio_init(BUTTON_COR_2);
    gpio_init(BUZZER_ACERTO);
    gpio_init(BUZZER_ERRO);
    gpio_init(LED_ERRO);
    gpio_init(LED_ACERTO);

    gpio_set_dir(JOYSTICK_VRX, GPIO_IN);
    gpio_set_dir(JOYSTICK_VRY, GPIO_IN);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_set_dir(BUTTON_COR_1, GPIO_IN);
    gpio_set_dir(BUTTON_COR_2, GPIO_IN);
    gpio_set_dir(BUZZER_ACERTO, GPIO_OUT);
    gpio_set_dir(BUZZER_ERRO, GPIO_OUT);
    gpio_set_dir(LED_ERRO, GPIO_OUT);
    gpio_set_dir(LED_ACERTO, GPIO_OUT);

    gpio_pull_up(JOYSTICK_SW);
    gpio_pull_up(BUTTON_COR_1);
    gpio_pull_up(BUTTON_COR_2);

    // Inicializar ADC
    adc_init();

    // Habilitar os pinos do ADC (GPIO 26 e 27)
    adc_gpio_init(JOYSTICK_VRY); // GPIO26 é ADC0
    adc_gpio_init(JOYSTICK_VRX); // GPIO27 é ADC1

    // Selecionar a entrada para o ADC (inicialmente selecionamos o VRX)
    adc_select_input(1); // ADC1

    npInit(LED_PIN);
    npClear();
    npWrite();

    srand(time(NULL));

    int nivel = 1;
    const int MAX_SEQUENCIA = 10;
     bool paused = false;  // Variável para controlar o estado de pausa

    while (true) {

         // Verificar se o botão do joystick foi pressionado
        if (lerBotaoJoystick()) {
            paused = !paused; // Alternar o estado de pausa

            if (paused) {
                printf("Jogo pausado!\n");
                // Adicione aqui qualquer ação visual para indicar que o jogo está pausado
                // Por exemplo, piscar todos os LEDs ou mostrar uma cor específica
            } else {
                printf("Jogo retomado!\n");
                 sleep_ms(1000);
                // Adicione aqui qualquer ação visual para indicar que o jogo foi retomado
            }

            // Esperar um pouco para evitar leituras múltiplas do botão
            sleep_ms(200);
        }
         if (!paused) {
            printf("Nível: %d\n", nivel);

            Direcao sequencia[MAX_SEQUENCIA];
            bool cores[MAX_SEQUENCIA];
            for (int i = 0; i < nivel; i++) {
                sequencia[i] = (Direcao)(rand() % 4);
                cores[i] = rand() % 2;
            }

            mostrarSequencia(sequencia, cores, nivel);

            printf("Sua vez!\n");
            bool acertou = verificarSequencia(sequencia, cores, nivel);

            if (acertou) {
                printf("Parabéns! Próximo nível.\n");
                tocarBuzzerAcerto(200);
                acenderLedAcerto(200);
                nivel++;
                printf("Prepare-se\n");
                sleep_ms(2000);

                if (nivel > MAX_SEQUENCIA){
                    printf("Você venceu o jogo!\n");
                    while(true){
                        npClear();
                        npWrite();
                    }
                }
            } else {
                printf("Errou! Game Over.\n");
                tocarBuzzerErro(500);
                acenderLedErro(500);
                sleep_ms(2000);
                nivel = 1;
            }
        }else {
             sleep_ms(100);
        }
    }

    return 0;
}