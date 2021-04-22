#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "counter.h"

CounterMsgs_state state;
bool initialized = false;

unsigned j = 0, k = 0;
unsigned responses = 0;

#define NUM_ITERS 100

void runHostAction() {
  Result_int16 sumResult;
  if (dequeue_CounterMsgs_sums(&state, &sumResult)) {
    printf("==== Sum %hhu: %hd ====\n", sumResult.id, sumResult.val);
    responses++;
  }
  Result_int32 productResult;
  if (dequeue_CounterMsgs_products(&state, &productResult)) {
    printf("==== Product %hhu: %hd ====\n", productResult.id, productResult.val);
    responses++;
  }

  for (unsigned i = 0; i < random() % 10; i++) {
    if (k >= NUM_ITERS) break;

    // Send 5 sequential Val commands, followed by a Reset
    Command command = {Command_Num, {.Num = {j + k * 5, j + 1}}};
    if (j == 5) {
      command.tag = Command_Reset;
      command.contents.Reset = k + 2;
    }
    if (enqueue_CounterMsgs_commands(&state, command)) {
      printf("Enqueued command\n");
      j++;
      if (j > 5) {
        j = 0;
        k++;
      }
    } else break;
  }

  // Halt after the expected number of commands have been recieved
  if (responses >= NUM_ITERS * 10) {
    enqueue_CounterMsgs_commands(&state, (Command){Command_Halt});
  }
}

uint8_t out_buf[size_ctob_CounterMsgs] = {0};
bool avail = false;

unsigned char messageAvailable() {
  if (!initialized) {
    init_CounterMsgs(&state);
    initialized = true;
  }

  // Run a step of the "host app" whenever availability is polled.
  // In reality this would be run from the same top-level event loop that is
  // listening on a serial port, or in a seperate thread of the host process.
  runHostAction();

  if (!avail) {
    size_t size = encode_CounterMsgs(&state, out_buf);
    avail = size > 0;
  }
  return avail;
}

unsigned long long getMessage() {
  if (!initialized) {
    init_CounterMsgs(&state);
    initialized = true;
  }

  if (!avail) {
    size_t size = encode_CounterMsgs(&state, out_buf);
    avail = size > 0;
  }

  //printf("C sending message %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n", out_buf[0], out_buf[1], out_buf[2], out_buf[3], out_buf[4], out_buf[5], out_buf[6]);

  unsigned long long out = 0;
  for (int i = 0; i < size_ctob_CounterMsgs; i++) {
    out <<= 8;
    out |= out_buf[i];
  }
  memset(out_buf, 0, sizeof(out_buf));
  avail = false;

  return out;
}

unsigned char putMessage(unsigned long long in) {
  if (!initialized) {
    init_CounterMsgs(&state);
    initialized = true;
  }

  uint8_t in_buf[size_btoc_CounterMsgs];
  for (int i = size_btoc_CounterMsgs - 1; i >= 0; i--) {
    in_buf[i] = in & 0xFF;
    in >>= 8;
  }

  //printf("C recieved message %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n", in_buf[0], in_buf[1], in_buf[2], in_buf[3], in_buf[4], in_buf[5], in_buf[6]);

  _Bool result = decode_CounterMsgs(&state, in_buf);
  return result;
}
