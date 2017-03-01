/**
 * Программа разработана Степановым Сергеем для микроконтроллера Atmega328P.
 * Может быть использована для Arduino pro_mini, Arduino UNO, Arduino nano.
 * Программа написана на языке C для сборки через avr-gcc.
 *
 * Программа для тестирования IR пульта от конструктора LEGO
 * Выход "инверсного" приемника/димодулятора IR сигнала должен быть подключен
 * к входу PB0 (pin 8 для Arduino). Использовался IR димодулятор TSOP4838
 *
 * Комуникации через порт RS232 на скорости 38400 bod.
 *
 * Программа принимает код через IR ресивер и выводит его в порт RS232
 * Виде двухбайтного HEX значения.
 * Процесс приема "идицируется" на pin PB5 (pin 13 Arduino)
 *
 * Компиляция:
 *    make
 *
 * Загрузка готовой прошивки:
 *    make pro_mini
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define F_CPU 16000000
#include "tools/uart_async.h"
#include "tools/legoIrFSM.h"
#define TIMERS_ARRAY_LEN 64

//============= Состояния уловителя импульсов  ===============
// Ловим спадающий фронт.
#define CST_START 0 
// Ловим нарастающий фронт.
#define CST_END 1 
uint8_t capState = CST_START;
uint8_t timeExtender = 0; // Дополнительный байт.для таймера.

/**
 * @description Прерывание верхнего значения таймера. Во избежании неоднозначных 
 *     ситуаций толжно выполняться не менее одного такта таймера (8 clk).
 */
ISR(TIMER1_COMPA_vect) {
  timeExtender++;
}

/**
 * @description Захват времени фронта импульса для PIN PB0.
 */
ISR(TIMER1_CAPT_vect) {
  uint16_t tim;
  TCNT1 = 0; // Максимально быстро сбрасываем основной таймер.

  if (timeExtender > 1) tim = 0xFFFF; //Если прошло два цикла таймера - мы на паузе.
  else tim = (ICR1 >> 1); // Переводим длительность импульса в микро секунды.

  timeExtender = 0; // Сброс таймера.

  legoIrFSM_upuls(tim, capState); // Передаем параметры импульса в парсер.

  // Если ловим спадающий фронт.
  if (capState == CST_START) 
  {
    // Переключаемся на ловлю нарастающего фронта.
    capState = CST_END;
    TCCR1B |= _BV(ICES1);
    
    PORTB |= _BV(PB5); // Подмигиваем.
  }
  else 
  {
    // Переключаемся на ловлю спадающиго фронта.
    capState = CST_START;
    TCCR1B &= ~_BV(ICES1); 

    PORTB &= ~_BV(PB5); //Подмигиваем.
  }
}

/**
 * @description функция обратного вызова, которая автоматически вызывается
 *       при завершении приема двух байт данных.
 *
 * @param value принятое значений
 * @param rCount количество повторов (не реализовано)
 */
void received (legoIrFSM_uCmd value, int8_t rCount) {
  static uint16_t prevRaw = 0;
  if (prevRaw == value.raw) return;
  prevRaw = value.raw;
//  uart_writelnHEXEx((unsigned char *)&value, 2); 
  uart_write("control: ");
  uart_writelnHEX(value.cmd.control);
  uart_write("chanel: ");
  uart_writelnHEX(value.cmd.chanel);
  uart_write("sequence: ");
  uart_writelnHEX(value.cmd.sequence);
  uart_write("direction ");
  uart_writelnHEX(value.cmd.direction);
  uart_writeln("");
}

/**
 * @description инициализация портов и таймера.
 */
void setup(){
  DDRB |= _BV(PB5); // Активировать PIN для светодиода arduino pro_mini.
  // Настройка Timer1/Counter.
  TCCR1A = 0; // CTC mode - TOP значения таймера в регистресе OCR1A.
  TCCR1B = _BV(WGM12); // CTC mode.
  // Делитель частоты счетчика 8 (CS12:10 - 010).
  // При частоте 16 MHz - 0,5 мкС один такт.
  TCCR1B |= _BV(CS11); 
  // Из расчета 0,5 мкС на такт, ставим длину таймера 25 мили секунд.
  // 40 раз в секунду. 50000 *0,5 = 25 000.
  OCR1A  = 50000;
  TIMSK1 = _BV(OCIE1A); // Включить прерывание TOP таймера.
  TIMSK1 |= _BV(ICIE1); // Прерывание захвата импульса.
}

int main(void) {
  setup();
  uart_async_init();
  // Инициализация парсера для LEGO пульта.
  legoIrFSM_reset();
  legoIrFSM_setCallBack(received);

  sei(); // Включить прерывания

  while(1) {
    sleep_mode();
  }
  return 0;
}
