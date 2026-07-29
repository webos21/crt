/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pthread.h>

static pthread_mutex_t gdtoa_map_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gdtoa_locks[2] = {
  PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER,
};
static void* gdtoa_lock_slots[2];

static pthread_mutex_t* gdtoa_lock_for_slot(void* lock_slot) {
  pthread_mutex_t* result = &gdtoa_locks[0];

  pthread_mutex_lock(&gdtoa_map_lock);
  for (int i = 0; i < 2; ++i) {
    if (gdtoa_lock_slots[i] == lock_slot) {
      result = &gdtoa_locks[i];
      break;
    }
    if (gdtoa_lock_slots[i] == 0) {
      gdtoa_lock_slots[i] = lock_slot;
      result = &gdtoa_locks[i];
      break;
    }
  }
  pthread_mutex_unlock(&gdtoa_map_lock);
  return result;
}

void __crt_gdtoa_mutex_lock(void* lock_slot) {
  pthread_mutex_lock(gdtoa_lock_for_slot(lock_slot));
}

void __crt_gdtoa_mutex_unlock(void* lock_slot) {
  pthread_mutex_unlock(gdtoa_lock_for_slot(lock_slot));
}
