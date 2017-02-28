/**
 * legoIrFSM.c
 */
#include <avr/io.h>
#include "uart_async.h"
#include "legoIrFSM.h"
#define NULL 0

typedef uint8_t (*state_callback)(enum states state, uint16_t len, uint8_t level);

legoIrFSM_received ir_callBack = NULL;
enum states legoIrFSM_state = FSM_ST_P;
uint8_t legoIrFSM_counter = 0xFF;

struct transition
{
    enum states nextState;
    state_callback fun;
};

// Проверка корректности временных диапазонов
uint8_t check_interval(enum states state, uint16_t len, uint8_t level) {
  switch (state) {
    case FSM_ST_P :
      return len > 10000 ? 0 : 1;
      break;
    case FSM_ST_SHIGH :
      return ((len > 150) && (len < 250)) ? 0 : 1;
      break;
    case FSM_ST_SLOW :
      return ((len > 900) && (len < 1100)) ? 0 : 1;
      break;
    case FSM_ST_H :
      return ((len > 150) && (len < 250)) ? 0 : 1;
      break;
    default : return 0;
  }
};

/**
 * @description Функция вызывается для обработки импульса содержащего значение бита
 *
 * @param state - состояние парсера (игнорируется)
 * @param len   - длительность импульса в микросекундах
 * @param level - уровень импульса (игнорируется)
 */
uint8_t save_value(enum states state, uint16_t len, uint8_t level) {
  legoIrFSM_counter++;
  if (legoIrFSM_counter > 15) return 1; // Прием данных был закончен - нужно сбросить состояние парсера

  // Запись значения принятого бита
  legoIrFSM_value = (legoIrFSM_value << 1);
  if ((len > 500) && (len < 600)) {
    legoIrFSM_value++;
  };

  // Зацикливаем состояния для обработки следующего бита
  legoIrFSM_state = FSM_ST_SLOW;

  // Если приняты биты 0-15 и определена процедура обратного вызова
  // вызват ьпользовательскую функцию.
  if ((legoIrFSM_counter == 15) && (legoIrFSM_value != NULL)) {
    ir_callBack(legoIrFSM_value, 1);
  };
  return 0;
}

/**
 * @description таблица правил изменения состояний парсера
 *    зацикливание состояний FSM_ST_H/FSM_ST_B осуществляется в функции.
 */
struct transition FSM_table[5] = {
    [FSM_ST_P] = {FSM_ST_SHIGH, check_interval},
    [FSM_ST_SHIGH] = {FSM_ST_SLOW, check_interval},
    [FSM_ST_SLOW] = {FSM_ST_H, check_interval},
    [FSM_ST_H] = {FSM_ST_B, check_interval},
    [FSM_ST_B] = {FSM_ST_P, save_value}
};

void legoIrFSM_reset(void) {
  legoIrFSM_state = FSM_ST_P;
  legoIrFSM_counter = 0xFF;
}

void legoIrFSM_upuls(uint16_t len, uint8_t level) {
  int8_t result = 0;
  // Если определена процедура - выполним ее
  if (FSM_table[legoIrFSM_state].fun != NULL) {
    result = FSM_table[legoIrFSM_state].fun(legoIrFSM_state, len, level);
  }

  // Если ошибка состояния
  if (result) {
    legoIrFSM_reset();
    // Если низкий уровень, ждем следующий синхро импульс для высокого, 
    // если всокий - ждем следующий синхро импульс на низком.
    legoIrFSM_state = (level ? FSM_ST_SLOW : FSM_ST_SHIGH);
  } else { // Или переходим на следующий шаг/состояние.
    legoIrFSM_state = FSM_table[legoIrFSM_state].nextState;
  }
}

void legoIrFSM_mpuls(uint8_t len, uint8_t level) {
  if (len > 65) len = 65;
  legoIrFSM_upuls(len * 1000, level);
}

void legoIrFSM_callback(legoIrFSM_received fn) {
  ir_callBack = fn;
}
