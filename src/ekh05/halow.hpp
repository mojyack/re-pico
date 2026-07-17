#pragma once

auto prepare_pins_for_halow() -> bool;

// mm8108 irq line (EXTI15) interrupt, notifies halow::chip_irq_event
extern "C" auto exti15_handler() -> void;
