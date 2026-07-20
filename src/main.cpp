/**
 * ATtiny85 Watchdog per Seeed Xiao nRF52840 (Meshtastic)
 * 
 * Funzionalità:
 * - Monitoraggio batteria con isteresi (SuperSmartReset style)
 * - Watchdog Xiao: reset se nessun heartbeat per ~10 minuti
 * - Delay riaccensione: ~10 minuti di stabilità prima di ripartire
 * - Deep sleep: consumo ~5-10 µA
 * 
 * Pinout ATtiny85 SOIC-8:
 * PB0 (pin 5) → RESET Xiao (active-low, open-drain style)
 * PB1 (pin 6) → Heartbeat da Xiao (input, toggle detection)
 * PB2 (pin 7) → ADC lettura Vbat (con partitore 100k+100k)
 * 
 * Soglie ADC (partitore 1:2, Vref=3.3V):
 * - 500 ≈ 3.2V batteria reale → SPEGNI
 * - 550 ≈ 3.5V batteria reale → ACCENDI (dopo attesa)
 */

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

// ========== PIN MAPPING ==========
#define PIN_RESET_OUT   0    // PB0 → Fisico 5 → RESET Xiao
#define PIN_HEARTBEAT   1    // PB1 → Fisico 6 → Input heartbeat
#define PIN_ADC_BAT     A1   // PB2 → Fisico 7 → ADC batteria

// ========== SOGLIE BATTERIA (con partitore 100k+100k) ==========
// Formula: ADC = (Vbat/2) / 3.3V × 1023
// 3.2V → ~496 → usiamo 500 (margini di sicurezza)
// 3.5V → ~542 → usiamo 550
const int THRESH_LOW   = 750; //500;  // ~3.2V → SPEGNI SUBITO
const int THRESH_HIGH  = 800; //550;  // ~3.5V → INIZIA CONTEGGIO RECUPERO

// ========== TIMING (Watchdog ~8 secondi per ciclo) ==========
const uint8_t WDT_CYCLES_DEAD     = 75;  // 75 × 8s = ~10 min senza heartbeat → reset
const uint8_t WDT_CYCLES_RECOVERY = 76;  // 76 × 8s = ~10 min attesa prima di riaccendere

// ========== STATI DEL SISTEMA ==========
enum SysState { ON, OFF, WAITING };

// ========== VARIABILI GLOBALI ==========
volatile bool wdt_fired = false;      // Flag interrupt watchdog
SysState systemState = ON;            // Stato corrente
uint8_t recovery_count = 0;           // Contatore attesa ricarica
uint8_t heartbeat_miss_count = 0;     // Contatore heartbeat persi
int last_heartbeat_val = HIGH;        // Ultimo valore letto su heartbeat

// ========== INTERRUPT WATCHDOG (~8 secondi) ==========
ISR(WDT_vect) {
  wdt_fired = true;
}

// ========== FUNZIONE RESET XIAO (Open-Drain style) ==========
void triggerReset() {
  // Porta il pin RESET a LOW per ~100ms
  pinMode(PIN_RESET_OUT, OUTPUT);
  digitalWrite(PIN_RESET_OUT, LOW);
  delay(100);
  // Rilascia: torna in INPUT, la pull-up interna del Xiao tiene alto
  pinMode(PIN_RESET_OUT, INPUT);
}

// ========== SETUP ==========
void setup() {
  // Configura pin RESET: parte in INPUT (rilasciato, Xiao RUN)
  pinMode(PIN_RESET_OUT, INPUT);
  
  // Configura heartbeat: input con pull-up interna
  pinMode(PIN_HEARTBEAT, INPUT_PULLUP);
  
  // Configura ADC: riferimento = VCC (1.1V)
  analogReference(INTERNAL);
  
  // Lampeggio di avvio (conferma che il firmware è partito)
  for (int i = 0; i < 2; i++) {
    pinMode(PIN_RESET_OUT, OUTPUT);
    digitalWrite(PIN_RESET_OUT, LOW);
    delay(150);
    pinMode(PIN_RESET_OUT, INPUT);
    delay(150);
  }

  // Configura Watchdog: interrupt ogni ~8 secondi
  MCUSR = 0;  // Clear reset flags
  WDTCR = (1 << WDCE) | (1 << WDE);              // Abilita modifica
  WDTCR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0); // ~8s, interrupt mode

  // Configura deep sleep: power-down (consumo minimo)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

// ========== LOOP PRINCIPALE ==========
void loop() {
  // Esegui logica solo quando il watchdog sveglia la CPU
  if (wdt_fired) {
    wdt_fired = false;
    
    // 🔋 1. LETTURA BATTERIA (ogni ~8 secondi)
    int vbat = analogRead(PIN_ADC_BAT);
    
    switch (systemState) {
      case ON:
        // Batteria scarica → spegni subito
        if (vbat < THRESH_LOW) {
          systemState = OFF;
          triggerReset();  // Reset Xiao
          recovery_count = 0;
        }
        break;
        
      case OFF:
        // Batteria recuperata → inizia attesa stabilità
        if (vbat > THRESH_HIGH) {
          systemState = WAITING;
          recovery_count = 0;
        }
        break;
        
      case WAITING:
        // Se crolla di nuovo → abortisci e resta spento
        if (vbat < THRESH_LOW) {
          systemState = OFF;
        } 
        else {
          // Altrimenti conta cicli di stabilità
          recovery_count++;
          if (recovery_count >= WDT_CYCLES_RECOVERY) {
            // Tempo scaduto → riaccendi con boot pulito
            systemState = ON;
            triggerReset();  // Rilascio del reset = boot pulito
          }
        }
        break;
    }
    
    // 💓 2. MONITORAGGIO HEARTBEAT XIAO
    int current_val = digitalRead(PIN_HEARTBEAT);
    
    // Se il segnale è CAMBIATO → Xiao è vivo!
    if (current_val != last_heartbeat_val) {
      heartbeat_miss_count = 0;           // Reset contatore errori
      last_heartbeat_val = current_val;   // Aggiorna riferimento
    } 
    else {
      // Nessun cambio da ~8 secondi...
      heartbeat_miss_count++;
      
      // Se superiamo la soglia (10 min) → Xiao bloccato → resetta
      if (heartbeat_miss_count >= WDT_CYCLES_DEAD && systemState == ON) {
        triggerReset();  // Forza riavvio
        heartbeat_miss_count = 0;  // Reset contatore
      }
    }
  }
  
  // 💤 DEEP SLEEP fino al prossimo interrupt watchdog
  sleep_enable();
  sei();              // ⚠️ FONDAMENTALE: riabilita interrupt globali
  sleep_cpu();        // CPU dorme qui, si sveglia SOLO per WDT
  sleep_disable();
}