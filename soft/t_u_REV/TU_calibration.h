#ifndef TU_CALIBRATION_H_
#define TU_CALIBRATION_H_

#include "TU_ADC.h"
#include "TU_config.h"
#include "TU_outputs.h"
#include "src/util_pagestorage.h"
#include "util/EEPROMStorage.h"

//#define VERBOSE_LUT
#ifdef VERBOSE_LUT
#define LUT_PRINTF(fmt, ...) serial_printf(fmt, ##__VA_ARGS__)
#else
#define LUT_PRINTF(x, ...) do {} while (0)
#endif

//#define CALIBRATION_LOAD_LEGACY

namespace TU {

enum CalibrationFlags {
  CALIBRATION_FLAG_ENCODERS_REVERSED = (0x1 << 0)
};

struct CalibrationData {
  static constexpr uint32_t FOURCC = FOURCC<'C', 'A', 'L', 1>::value;

  OUTPUTS::CalibrationData dac;
  ADC::CalibrationData adc;

  uint8_t display_offset;
  uint32_t flags;
  uint32_t reserved0;
  uint32_t reserved1;

  bool encoders_reversed() const {
  	return flags & CALIBRATION_FLAG_ENCODERS_REVERSED;
  }

  void reverse_encoders() {
    if (flags & CALIBRATION_FLAG_ENCODERS_REVERSED)
      flags &= ~flags & CALIBRATION_FLAG_ENCODERS_REVERSED;
    else
      flags |= CALIBRATION_FLAG_ENCODERS_REVERSED;
  }
};

typedef PageStorage<EEPROMStorage, EEPROM_CALIBRATIONDATA_START, EEPROM_CALIBRATIONDATA_END, CalibrationData> CalibrationStorage;

extern CalibrationData calibration_data;

}; // namespace TU

// Forward declarations for screwy build system
struct CalibrationStep;
struct CalibrationState;

#endif // TU_CALIBRATION_H_
