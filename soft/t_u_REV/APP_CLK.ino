// Copyright (c) 2015, 2016 Max Stadler, Patrick Dowling
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "util/util_settings.h"
#include "util/util_turing.h"
#include "util/util_logistic_map.h"
#include "TU_apps.h"
#include "TU_outputs.h"
#include "TU_menus.h"
#include "TU_strings.h"
#include "TU_visualfx.h"
#include "TU_pattern_edit.h"
#include "TU_patterns.h"
#include "extern/dspinst.h"

namespace menu = TU::menu; 

const uint8_t MODES = 7;        // # clock modes
const uint8_t DAC_MODES = 4;    // # DAC submodes
const uint8_t RND_MAX = 31;     // max random (n)
const uint8_t MULT_MAX = 18;    // max multiplier
const uint8_t MULT_BY_ONE = 11; // default multiplication  
const uint8_t PULSEW_MAX = 255; // max pulse width [ms]
const uint8_t BPM_MIN = 1;      // changes need changes in TU_BPM.h
const int16_t BPM_MAX = 320;    // ditto
const uint8_t LFSR_MAX = 31;    // max LFSR length
const uint8_t LFSR_MIN = 4;     // min "
const uint8_t EUCLID_N_MAX = 32; 
const uint16_t TOGGLE_THRESHOLD = 500; // ADC threshold for 0/1 parameters (500 = ~1.2V)

const uint32_t SCALE_PULSEWIDTH = 58982; // 0.9 for signed_multiply_32x16b
const uint32_t TICKS_TO_MS = 43691; // 0.6667f : fraction, if TU_CORE_TIMER_RATE = 60 us : 65536U * ((1000 / TU_CORE_TIMER_RATE) - 16)
const uint32_t TICK_JITTER = 0xFFFFFFF;  // 1/16 : threshold/double triggers reject -> ext_frequency_in_ticks_
const uint32_t TICK_SCALE  = 0xC0000000; // 0.75 for signed_multiply_32x32                      

extern const uint32_t BPM_microseconds_4th[];

uint32_t ticks_src1 = 0; // main clock frequency (top)
uint32_t ticks_src2 = 0; // sec. clock frequency (bottom)
bool RESYNC = true;      // resync internal timers

const uint64_t multipliers_[] = {
  
  0xFFFFFFFF, // x1
  0x80000000, // x2
  0x55555555, // x3
  0x40000000, // x4
  0x33333333, // x5
  0x2AAAAAAB, // x6
  0x24924925, // x7
  0x20000000  // x8

}; // = 2^32 / multiplier

const uint64_t pw_scale_[] = {
  
  0xFFFFFFFF, // /64
  0xC0000000, // /48
  0x80000000, // /32
  0x40000000, // /16
  0x20000000, // /8
  0x1C000000, // /7
  0x18000000, // /6
  0x14000000, // /5
  0x10000000, // /4
  0xC000000,  // /3
  0x8000000,  // /2
  0x4000000,  // x1

}; // = 2^32 * divisor / 64

const uint8_t divisors_[] = {

   64,
   48,
   32,
   16,
   8,
   7,
   6,
   5,
   4,
   3,
   2,
   1
};

enum ChannelSetting { 
  // shared
  CHANNEL_SETTING_MODE,
  CHANNEL_SETTING_MODE4,
  CHANNEL_SETTING_CLOCK,
  CHANNEL_SETTING_RESET,
  CHANNEL_SETTING_MULT,
  CHANNEL_SETTING_PULSEWIDTH,
  CHANNEL_SETTING_INTERNAL_CLK,
  // mode specific
  CHANNEL_SETTING_LFSR_TAP1,
  CHANNEL_SETTING_LFSR_TAP2,
  CHANNEL_SETTING_RAND_N,
  CHANNEL_SETTING_EUCLID_N,
  CHANNEL_SETTING_EUCLID_K,
  CHANNEL_SETTING_EUCLID_OFFSET,
  CHANNEL_SETTING_LOGIC_TYPE,
  CHANNEL_SETTING_LOGIC_OP1,
  CHANNEL_SETTING_LOGIC_OP2,
  CHANNEL_SETTING_LOGIC_TRACK_WHAT,
  CHANNEL_SETTING_DAC_RANGE,
  CHANNEL_SETTING_DAC_MODE,
  CHANNEL_SETTING_DAC_TRACK_WHAT,
  CHANNEL_SETTING_HISTORY_WEIGHT,
  CHANNEL_SETTING_HISTORY_DEPTH,
  CHANNEL_SETTING_TURING_LENGTH,
  CHANNEL_SETTING_TURING_PROB,
  CHANNEL_SETTING_LOGISTIC_MAP_R,
  CHANNEL_SETTING_MASK1,
  CHANNEL_SETTING_MASK2,
  CHANNEL_SETTING_MASK3,
  CHANNEL_SETTING_MASK4,
  CHANNEL_SETTING_SEQUENCE,
  CHANNEL_SETTING_SEQUENCE_LEN1,
  CHANNEL_SETTING_SEQUENCE_LEN2,
  CHANNEL_SETTING_SEQUENCE_LEN3,
  CHANNEL_SETTING_SEQUENCE_LEN4,
  CHANNEL_SETTING_SEQUENCE_PLAYMODE,
  // cv sources
  CHANNEL_SETTING_MULT_CV_SOURCE,
  CHANNEL_SETTING_PULSEWIDTH_CV_SOURCE,
  CHANNEL_SETTING_CLOCK_CV_SOURCE,
  CHANNEL_SETTING_LFSR_TAP1_CV_SOURCE,
  CHANNEL_SETTING_LFSR_TAP2_CV_SOURCE,
  CHANNEL_SETTING_RAND_N_CV_SOURCE,
  CHANNEL_SETTING_EUCLID_N_CV_SOURCE,
  CHANNEL_SETTING_EUCLID_K_CV_SOURCE,
  CHANNEL_SETTING_EUCLID_OFFSET_CV_SOURCE,
  CHANNEL_SETTING_LOGIC_TYPE_CV_SOURCE,
  CHANNEL_SETTING_LOGIC_OP1_CV_SOURCE,
  CHANNEL_SETTING_LOGIC_OP2_CV_SOURCE,
  CHANNEL_SETTING_TURING_PROB_CV_SOURCE,
  CHANNEL_SETTING_TURING_LENGTH_CV_SOURCE,
  CHANNEL_SETTING_LOGISTIC_MAP_R_CV_SOURCE,
  CHANNEL_SETTING_SEQ_CV_SOURCE,
  CHANNEL_SETTING_MASK_CV_SOURCE,
  CHANNEL_SETTING_DAC_RANGE_CV_SOURCE,
  CHANNEL_SETTING_DAC_MODE_CV_SOURCE,
  CHANNEL_SETTING_HISTORY_WEIGHT_CV_SOURCE,
  CHANNEL_SETTING_HISTORY_DEPTH_CV_SOURCE,
  CHANNEL_SETTING_DUMMY,
  CHANNEL_SETTING_DUMMY_EMPTY,
  CHANNEL_SETTING_SCREENSAVER,
  CHANNEL_SETTING_LAST
};
  
enum ChannelTriggerSource {
  CHANNEL_TRIGGER_TR1,
  CHANNEL_TRIGGER_TR2,
  CHANNEL_TRIGGER_NONE,
  CHANNEL_TRIGGER_INTERNAL,
  CHANNEL_TRIGGER_LAST,
  CHANNEL_TRIGGER_FREEZE_HIGH = CHANNEL_TRIGGER_INTERNAL,
  CHANNEL_TRIGGER_FREEZE_LOW = CHANNEL_TRIGGER_LAST
};

enum ChannelCV_Mapping {
  CHANNEL_CV_MAPPING_CV1,
  CHANNEL_CV_MAPPING_CV2,
  CHANNEL_CV_MAPPING_CV3,
  CHANNEL_CV_MAPPING_CV4,
  CHANNEL_CV_MAPPING_LAST
};

enum CLOCKMODES {
  MULT,
  LFSR,
  RANDOM,
  EUCLID,
  LOGIC,
  SEQ,
  DAC,
  LAST_MODE
};

enum DACMODES {
  _BINARY,
  _RANDOM,
  _TURING,
  _LOGISTIC,
  LAST_DACMODE
};

enum CLOCKSTATES {
  OFF,
  ON = 4095
};

enum LOGICMODES {
  AND,
  OR,
  XOR,
  XNOR,
  NAND,
  NOR,
  LOGICMODE_LAST
};

enum MENUPAGES {
  PARAMETERS,
  CV_SOURCES,
  TEMPO
};

uint64_t ext_frequency[CHANNEL_TRIGGER_LAST];

class Clock_channel : public settings::SettingsBase<Clock_channel, CHANNEL_SETTING_LAST> {
public:

  uint8_t get_mode() const {
    return values_[CHANNEL_SETTING_MODE];
  }

  uint8_t get_mode4() const {
    return values_[CHANNEL_SETTING_MODE4];
  }

  uint8_t get_clock_source() const {
    return values_[CHANNEL_SETTING_CLOCK];
  }

  void set_clock_source(uint8_t _src) {
    apply_value(CHANNEL_SETTING_CLOCK, _src);
  }
  
  int8_t get_multiplier() const {
    return values_[CHANNEL_SETTING_MULT];
  }

  uint16_t get_pulsewidth() const {
    return values_[CHANNEL_SETTING_PULSEWIDTH];
  }

  uint16_t get_internal_timer() const {
    return values_[CHANNEL_SETTING_INTERNAL_CLK];
  }

  uint8_t get_reset_source() const {
    return values_[CHANNEL_SETTING_RESET];
  }

  void set_reset_source(uint8_t src) {
    apply_value(CHANNEL_SETTING_RESET, src);
  }

  uint8_t get_tap1() const {
    return values_[CHANNEL_SETTING_LFSR_TAP1];
  }

  void set_tap1(uint8_t _t) {
    apply_value(CHANNEL_SETTING_LFSR_TAP1, _t);
  }

  uint8_t get_tap2() const {
    return values_[CHANNEL_SETTING_LFSR_TAP2];
  }

  void set_tap2(uint8_t _t) {
    apply_value(CHANNEL_SETTING_LFSR_TAP2, _t);
  }

  uint8_t rand_n() const {
    return values_[CHANNEL_SETTING_RAND_N];
  }

  uint8_t euclid_n() const {
    return values_[CHANNEL_SETTING_EUCLID_N];
  }

  uint8_t euclid_k() const {
    return values_[CHANNEL_SETTING_EUCLID_K];
  }

  void set_euclid_k(uint8_t k) {
    apply_value(CHANNEL_SETTING_EUCLID_K, k);
  }

  uint8_t euclid_offset() const {
    return values_[CHANNEL_SETTING_EUCLID_OFFSET];
  }

  void set_euclid_offset(uint8_t o) {
    apply_value(CHANNEL_SETTING_EUCLID_OFFSET, o);
  }

  uint8_t logic_type() const {
    return values_[CHANNEL_SETTING_LOGIC_TYPE];
  }

  uint8_t logic_op1() const {
    return values_[CHANNEL_SETTING_LOGIC_OP1];
  }

  uint8_t logic_op2() const {
    return values_[CHANNEL_SETTING_LOGIC_OP2];
  }

  uint8_t logic_tracking() const {
    return values_[CHANNEL_SETTING_LOGIC_TRACK_WHAT];
  }

  uint8_t dac_range() const {
    return values_[CHANNEL_SETTING_DAC_RANGE];
  }

  uint8_t dac_mode() const {
    return values_[CHANNEL_SETTING_DAC_MODE];
  }

  uint8_t binary_tracking() const {
    return values_[CHANNEL_SETTING_DAC_TRACK_WHAT];
  }

  uint8_t get_history_weight() const {
    return values_[CHANNEL_SETTING_HISTORY_WEIGHT];
  }

  uint8_t get_history_depth() const {
    return values_[CHANNEL_SETTING_HISTORY_DEPTH];
  }
  
  uint8_t get_turing_length() const {
    return values_[CHANNEL_SETTING_TURING_LENGTH];
  }

  uint8_t get_turing_probability() const {
    return values_[CHANNEL_SETTING_TURING_PROB];
  }

  uint8_t get_logistic_map_r() const {
    return values_[CHANNEL_SETTING_LOGISTIC_MAP_R];
  }

  int get_sequence() const {
    return values_[CHANNEL_SETTING_SEQUENCE];
  }

  int get_playmode() const {
    return values_[CHANNEL_SETTING_SEQUENCE_PLAYMODE];
  }

  int get_display_sequence() const {
    return display_sequence_;
  }

  void set_display_sequence(uint8_t seq) {
    display_sequence_ = seq;
  }

  int get_display_mask() const {
    return display_mask_;
  }

  void set_sequence(uint8_t seq) {
    apply_value(CHANNEL_SETTING_SEQUENCE, seq);
  }

  uint8_t get_sequence_length(uint8_t _seq) const {
    
    switch (_seq) {

    case 1:
    return values_[CHANNEL_SETTING_SEQUENCE_LEN2];
    break;
    case 2:
    return values_[CHANNEL_SETTING_SEQUENCE_LEN3];
    break;
    case 3:
    return values_[CHANNEL_SETTING_SEQUENCE_LEN4];
    break;    
    default:
    return values_[CHANNEL_SETTING_SEQUENCE_LEN1];
    break;
    }
  }
  
  uint8_t get_mult_cv_source() const {
    return values_[CHANNEL_SETTING_MULT_CV_SOURCE];
  }

  uint8_t get_pulsewidth_cv_source() const {
    return values_[CHANNEL_SETTING_PULSEWIDTH_CV_SOURCE];
  }

  uint8_t get_clock_source_cv_source() const {
    return values_[CHANNEL_SETTING_CLOCK_CV_SOURCE];
  }

  uint8_t get_tap1_cv_source() const {
    return values_[CHANNEL_SETTING_LFSR_TAP1_CV_SOURCE];
  }

  uint8_t get_tap2_cv_source() const {
    return values_[CHANNEL_SETTING_LFSR_TAP2_CV_SOURCE];
  }

  uint8_t get_rand_n_cv_source() const {
    return values_[CHANNEL_SETTING_RAND_N_CV_SOURCE];
  }

  uint8_t get_euclid_n_cv_source() const {
    return values_[CHANNEL_SETTING_EUCLID_N_CV_SOURCE];
  }

  uint8_t get_euclid_k_cv_source() const {
    return values_[CHANNEL_SETTING_EUCLID_K_CV_SOURCE];
  }

  uint8_t get_euclid_offset_cv_source() const {
    return values_[CHANNEL_SETTING_EUCLID_OFFSET_CV_SOURCE];
  }

  uint8_t get_logic_type_cv_source() const {
    return values_[CHANNEL_SETTING_LOGIC_TYPE_CV_SOURCE];
  }

  uint8_t get_logic_op1_cv_source() const {
    return values_[CHANNEL_SETTING_LOGIC_OP1_CV_SOURCE];
  }

  uint8_t get_logic_op2_cv_source() const {
    return values_[CHANNEL_SETTING_LOGIC_OP2_CV_SOURCE];
  }

  uint8_t get_turing_prob_cv_source() const {
    return values_[CHANNEL_SETTING_TURING_PROB_CV_SOURCE];
  }

  uint8_t get_turing_length_cv_source() const {
    return values_[CHANNEL_SETTING_TURING_LENGTH_CV_SOURCE];
  }

  uint8_t get_logistic_map_r_cv_source() const {
    return values_[CHANNEL_SETTING_LOGISTIC_MAP_R_CV_SOURCE];
  }

  uint8_t get_mask_cv_source() const {
    return values_[CHANNEL_SETTING_MASK_CV_SOURCE];
  }

  uint8_t get_sequence_cv_source() const {
    return values_[CHANNEL_SETTING_SEQ_CV_SOURCE];
  }

  uint8_t get_DAC_mode_cv_source() const {
    return values_[CHANNEL_SETTING_DAC_MODE_CV_SOURCE];
  }

  uint8_t get_DAC_range_cv_source() const {
    return values_[CHANNEL_SETTING_DAC_RANGE_CV_SOURCE];
  }

  uint8_t get_rand_history_w_cv_source() const {
    return values_[CHANNEL_SETTING_HISTORY_WEIGHT_CV_SOURCE];
  }

  uint8_t get_rand_history_d_cv_source() const {
    return values_[CHANNEL_SETTING_HISTORY_DEPTH_CV_SOURCE];
  }
  
  void update_pattern_mask(uint16_t mask, uint8_t sequence) {

    switch(sequence) {
   
    case 1:
      apply_value(CHANNEL_SETTING_MASK2, mask);
      break;
    case 2:
      apply_value(CHANNEL_SETTING_MASK3, mask);
      break;
    case 3:
      apply_value(CHANNEL_SETTING_MASK4, mask);
      break;    
    default:
      apply_value(CHANNEL_SETTING_MASK1, mask);
      break;   
    }
  }
  
  int get_mask(uint8_t _this_sequence) const {

    switch(_this_sequence) {
      
    case 1:
      return values_[CHANNEL_SETTING_MASK2];
      break;
    case 2:
      return values_[CHANNEL_SETTING_MASK3];
      break;
    case 3:
      return values_[CHANNEL_SETTING_MASK4];
      break;    
    default:
      return values_[CHANNEL_SETTING_MASK1];
      break;   
    }
  }
  
  void set_sequence_length(uint8_t len, uint8_t seq) {

    switch(seq) {
      case 0:
      apply_value(CHANNEL_SETTING_SEQUENCE_LEN1, len);
      break;
      case 1:
      apply_value(CHANNEL_SETTING_SEQUENCE_LEN2, len);
      break;
      case 2:
      apply_value(CHANNEL_SETTING_SEQUENCE_LEN3, len);
      break;
      case 3:
      apply_value(CHANNEL_SETTING_SEQUENCE_LEN4, len);
      break;
      default:
      break;
    }
  }

  uint16_t get_clock_cnt() const {
    return clk_cnt_;
  }

  void update_internal_timer(uint16_t _tempo) {
      apply_value(CHANNEL_SETTING_INTERNAL_CLK, _tempo);
  }

  uint8_t getTriggerState() const {
    return clock_display_.getState();
  }

  uint32_t get_logistic_map_register() const {
    return logistic_map_.get_register();
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  void pattern_changed(uint16_t mask) {
    force_update_ = true;
    if (get_sequence() == sequence_last_)
      display_mask_ = mask;
  }
  
  void clear_CV_mapping() {

    apply_value(CHANNEL_SETTING_PULSEWIDTH_CV_SOURCE, 0);
    apply_value(CHANNEL_SETTING_MULT_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_CLOCK_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LFSR_TAP1_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LFSR_TAP2_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_RAND_N_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_EUCLID_N_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_EUCLID_K_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_EUCLID_OFFSET_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LOGIC_TYPE_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LOGIC_OP1_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LOGIC_OP2_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_TURING_PROB_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_TURING_LENGTH_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_LOGISTIC_MAP_R_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_SEQ_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_MASK_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_DAC_RANGE_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_DAC_MODE_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_HISTORY_WEIGHT_CV_SOURCE,0);
    apply_value(CHANNEL_SETTING_HISTORY_DEPTH_CV_SOURCE,0);
  }

  void sync() {
    clk_cnt_ = 0x0;
    div_cnt_ = 0x0;
  }

  void reset_ticks_internal() {
    ticks_ = 0x0;
  }
  
  uint8_t get_page() const {
    return menu_page_;  
  }

  void set_page(uint8_t _page) {
    menu_page_ = _page;  
  }

  uint16_t get_zero(uint8_t channel) const {

    uint16_t _off = 0;
    if (channel == CLOCK_CHANNEL_4)
      _off += _ZERO;
      
    return _off; 
  }
  
  void Init(ChannelTriggerSource trigger_source) {
    
    InitDefaults();
    apply_value(CHANNEL_SETTING_CLOCK, trigger_source);

    force_update_ = true;
    gpio_state_ = OFF;
    ticks_ = 0;
    subticks_ = 0;
    tickjitter_ = 10000;
    clk_cnt_ = 0;
    clk_src_ = get_clock_source();
    reset_ = false;
    reset_counter_ = false;
    reset_me_ = false;
    logic_ = false;
    menu_page_ = PARAMETERS;
 
    prev_multiplier_ = get_multiplier();
    prev_pulsewidth_ = get_pulsewidth();
    bpm_last_ = 0;
    
    ext_frequency_in_ticks_ = get_pulsewidth() << 15; // init to something...
    channel_frequency_in_ticks_ = get_pulsewidth() << 15;
    pulse_width_in_ticks_ = get_pulsewidth() << 10;
     
    _ZERO = TU::calibration_data.dac.calibrated_Zero[0x0][0x0];

    // WTF? get_mask doesn't return the saved mask
    display_sequence_ = get_sequence();
    display_mask_ = 0; // get_mask(display_sequence_);
    sequence_last_ = display_sequence_;
    sequence_advance_ = false;
    sequence_advance_state_ = false; 
    
    turing_machine_.Init();
    turing_machine_.Clock();
    logistic_map_.Init();
    uint32_t _seed = TU::ADC::value<ADC_CHANNEL_1>() + TU::ADC::value<ADC_CHANNEL_2>() + TU::ADC::value<ADC_CHANNEL_3>() + TU::ADC::value<ADC_CHANNEL_4>();
    randomSeed(_seed);
    logistic_map_.set_seed(_seed);
    turing_machine_.set_shift_register(_seed);
    clock_display_.Init();
    update_enabled_settings(0);  
  }
 
  void force_update() {
    force_update_ = true;
  }

  /* main channel update below: */
   
  inline void Update(uint32_t triggers, CLOCK_CHANNEL clock_channel) {

     // increment channel ticks .. 
     subticks_++; 
     
     int8_t _clock_source, _reset_source, _mode;
     int8_t _multiplier;
     bool _none, _triggered, _tock, _sync;
     uint16_t _output = gpio_state_;
     uint32_t prev_channel_frequency_in_ticks_ = 0x0;

     // core channel parameters -- 
     // 1. clock source:
     _clock_source = get_clock_source();
     
     if (get_clock_source_cv_source()){
        int16_t _toggle = TU::ADC::value(static_cast<ADC_CHANNEL>(get_clock_source_cv_source() - 1));

        if (_toggle > TOGGLE_THRESHOLD && _clock_source <= CHANNEL_TRIGGER_TR2) 
          _clock_source = (~_clock_source) & 1u;
     }
     // 2. multiplication:
     _multiplier = get_multiplier();

     if (get_mult_cv_source()) {
        _multiplier += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_mult_cv_source() - 1)) + 127) >> 8;             
        CONSTRAIN(_multiplier, 0, MULT_MAX);
     }
     // 3. channel mode? 
     _mode = (clock_channel != CLOCK_CHANNEL_4) ? get_mode() : get_mode4();
     // clocked ?
     _none = CHANNEL_TRIGGER_NONE == _clock_source;
     _triggered = !_none && (triggers & DIGITAL_INPUT_MASK(_clock_source - CHANNEL_TRIGGER_TR1));
     _tock = false;
     _sync = false;
     
     // new tick frequency, external:
     if (_clock_source <= CHANNEL_TRIGGER_TR2) {
      
         if (_triggered || clk_src_ != _clock_source) {   
            ext_frequency_in_ticks_ = ext_frequency[_clock_source]; 
            _tock = true;
            div_cnt_--;
         }
     }
     // or else, internal clock active?
     else if (_clock_source == CHANNEL_TRIGGER_INTERNAL) {

           // resync? 
          if (_clock_source != clk_src_)
            RESYNC = true;
          ticks_++;
          _triggered = false;
          
          uint16_t _bpm = get_internal_timer() - BPM_MIN; // substract min value
          
          if (_bpm != bpm_last_ || clk_src_ != _clock_source) {
            // new BPM value, recalculate channel frequency below ... 
            ext_frequency_in_ticks_ = BPM_microseconds_4th[_bpm];
            _tock = true;
          }
          // store current bpm value
          bpm_last_ = _bpm; 
          
          // simulate clock ... ?
          if (ticks_ > ext_frequency_in_ticks_) {
            _triggered |= true;
            ticks_ = 0x0;
            div_cnt_--;
          }
     }     
     // store clock source:
     clk_src_ = _clock_source;
 
     // new multiplier ?
     if (prev_multiplier_ != _multiplier)
       _tock |= true;  
     prev_multiplier_ = _multiplier; 

     // if so, recalculate channel frequency and corresponding jitter-thresholds:
     if (_tock) {

        // when multiplying, skip too closely spaced triggers:
        if (_multiplier > MULT_BY_ONE) {
           prev_channel_frequency_in_ticks_ = multiply_u32xu32_rshift32(channel_frequency_in_ticks_, TICK_SCALE);
           // new frequency:    
           channel_frequency_in_ticks_ = multiply_u32xu32_rshift32(ext_frequency_in_ticks_, multipliers_[_multiplier-MULT_BY_ONE]); 
        }
        else {
           prev_channel_frequency_in_ticks_ = 0x0;
           // new frequency (used for pulsewidth):
           channel_frequency_in_ticks_ = multiply_u32xu32_rshift32(ext_frequency_in_ticks_, pw_scale_[_multiplier]) << 6;
        }
           
        tickjitter_ = multiply_u32xu32_rshift32(channel_frequency_in_ticks_, TICK_JITTER);  
     }
     // limit frequency to > 0
     if (!channel_frequency_in_ticks_)  
        channel_frequency_in_ticks_ = 1u;
          
     // reset? 
     _reset_source = get_reset_source();
     
     if (_reset_source < CHANNEL_TRIGGER_NONE && reset_me_) {

        uint8_t reset_state_ = !_reset_source ? digitalReadFast(TR1) : digitalReadFast(TR2);

        // ?
        if (reset_state_ < reset_) {
           div_cnt_ = 0x0;
           reset_counter_ = true; // reset clock counter below
           reset_me_ = false;
        }
        reset_ = reset_state_;
     } 

     // in sequencer mode, do we advance sequences by TR2?
     if (_mode == SEQ && get_playmode() > 3) {

        uint8_t _advance_trig = digitalReadFast(TR2);
        // ?
        if (_advance_trig < sequence_advance_state_) 
          sequence_advance_ = true;
          
        sequence_advance_state_ = _advance_trig;  
       
     }
       
    /*             
     *  brute force ugly sync hack:
     *  this, presumably, is needlessly complicated. 
     *  but seems to work ok-ish, w/o too much jitter and missing clocks... 
     */
     uint32_t _subticks = subticks_;

     if (_multiplier <= MULT_BY_ONE && _triggered && div_cnt_ <= 0) { 
        // division, so we track
        _sync = true;
        div_cnt_ = divisors_[_multiplier]; 
        subticks_ = channel_frequency_in_ticks_; // force sync
     }
     else if (_multiplier <= MULT_BY_ONE && _triggered) {
        // division, mute output:
        TU::OUTPUTS::setState(clock_channel, OFF);
     }
     else if (_multiplier > MULT_BY_ONE && _triggered)  {
        // multiplication, force sync, if clocked:
        _sync = true;
        subticks_ = channel_frequency_in_ticks_; 
     }
     else if (_multiplier > MULT_BY_ONE)
        _sync = true;   
     // end of ugly hack
     
     // time to output ? 
     if (subticks_ >= channel_frequency_in_ticks_ && _sync) { 
         
         // if so, reset ticks: 
         subticks_ = 0x0;
         // if tempo changed, reset _internal_ clock counter:
         if (_tock)
            ticks_ = 0x0;
     
         //reject, if clock is too jittery or skip quasi-double triggers when ext. frequency increases:
         if (_subticks < tickjitter_ || (_subticks < prev_channel_frequency_in_ticks_ && reset_me_)) 
            return;
            
         // mute output ?
         if (_reset_source > CHANNEL_TRIGGER_NONE) {
          
             if (_reset_source == CHANNEL_TRIGGER_FREEZE_HIGH && !digitalReadFast(TR2))
              return;
             else if (_reset_source == CHANNEL_TRIGGER_FREEZE_LOW && digitalReadFast(TR2)) 
              return;
         }
                     
         // only then count clocks:  
         clk_cnt_++;  
         
         // reset counter ? (SEQ/Euclidian)
         if (reset_counter_) 
            clk_cnt_ = 0x0;
     
         // clear for reset:
         reset_me_ = true;
         reset_counter_ = false;
         // finally, process trigger + output
         _output = gpio_state_ = process_clock_channel(_mode); // = either ON, OFF, or anything (DAC)
         if (_triggered) {
           TU::OUTPUTS::setState(clock_channel, _output);
         }
     }
  
     /*
      *  below: pulsewidth stuff
      */
      
     if (gpio_state_ && _mode != DAC) { 
       
        // pulsewidth setting -- 
        int16_t _pulsewidth = get_pulsewidth();

        if (_pulsewidth || _multiplier > MULT_BY_ONE || _clock_source == CHANNEL_TRIGGER_INTERNAL) {

            bool _gates = false;

            // do we echo && multiply? if so, do half-duty cycle:
            if (!_pulsewidth)
                _pulsewidth = PULSEW_MAX;
                
            if (_pulsewidth == PULSEW_MAX)
              _gates = true;
            // CV?
            if (get_pulsewidth_cv_source()) {

              _pulsewidth += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_pulsewidth_cv_source() - 1)) + 8) >> 3; 
              if (!_gates)          
                CONSTRAIN(_pulsewidth, 1, PULSEW_MAX);
              else // CV for 50% duty cycle: 
                CONSTRAIN(_pulsewidth, 1, (PULSEW_MAX<<1) - 55);  // incl margin, max < 2x mult. see below 
            }
            // recalculate (in ticks), if new pulsewidth setting:
            if (prev_pulsewidth_ != _pulsewidth || ! subticks_) {
                if (!_gates) {
                  int32_t _fraction = signed_multiply_32x16b(TICKS_TO_MS, static_cast<int32_t>(_pulsewidth)); // = * 0.6667f
                  _fraction = signed_saturate_rshift(_fraction, 16, 0);
                  pulse_width_in_ticks_  = (_pulsewidth << 4) + _fraction;
                }
                else { // put out gates/half duty cycle:
                  pulse_width_in_ticks_ = channel_frequency_in_ticks_ >> 1;
                  
                  if (_pulsewidth != PULSEW_MAX) { // CV?
                    pulse_width_in_ticks_ = signed_multiply_32x16b(static_cast<int32_t>(_pulsewidth) << 8, pulse_width_in_ticks_); // 
                    pulse_width_in_ticks_ = signed_saturate_rshift(pulse_width_in_ticks_, 16, 0);
                  }
                }
            }
            prev_pulsewidth_ = _pulsewidth;
            
            // limit pulsewidth, if approaching half duty cycle:
            if (!_gates && pulse_width_in_ticks_ >= channel_frequency_in_ticks_>>1) 
              pulse_width_in_ticks_ = (channel_frequency_in_ticks_ >> 1) | 1u;
              
            // turn off output? 
            if (subticks_ >= pulse_width_in_ticks_) 
              _output = gpio_state_ = OFF;
            else // keep on 
              _output = ON; 
         }
         else {
            // we simply echo the pulsewidth:
            bool _state = (_clock_source == CHANNEL_TRIGGER_TR1) ? !digitalReadFast(TR1) : !digitalReadFast(TR2);  
           
            if (_state)
              _output = ON; 
            else  
              _output = gpio_state_ = OFF;
         }   
     }
     // DAC channel needs extra treatment / zero offset: 
     if (clock_channel == CLOCK_CHANNEL_4 && _mode != DAC && gpio_state_ == OFF)
       _output += _ZERO;
       
     // update (physical) outputs:
     TU::OUTPUTS::set(clock_channel, _output);
  } // end update

  /* details re: trigger processing happens (mostly) here: */
  inline uint16_t process_clock_channel(uint8_t mode) {
 
      uint16_t _out = ON;
      logic_ = false;
  
      switch (mode) {
  
          case MULT:
            break;
          case LOGIC:
          // logic happens elsewhere.
            logic_ = true;
            break;
          case LFSR: {
              // LFSR, sort of. mash-up of o_C and old TU firmware:
              int16_t _length, _probability, _myfirstbit;
              
              _length = get_turing_length();
              _probability = get_turing_probability();

              if (get_turing_length_cv_source()) {
                _length += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_turing_length_cv_source() - 1)) + 64) >> 6;   
                CONSTRAIN(_length, LFSR_MIN, LFSR_MAX);
              }

              if (get_turing_prob_cv_source()) {
                _probability += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_turing_prob_cv_source() - 1)) + 16) >> 4;
                CONSTRAIN(_probability, 1, 255);              
              }

              turing_machine_.set_length(_length);
              turing_machine_.set_probability(_probability); 
              
              uint32_t _shift_register = turing_machine_.Clock();
  
              _myfirstbit =  _shift_register & 1u; // --> this is our output
              _out = _myfirstbit ? ON : OFF; // DAC needs special care ... 
              
              // now update LFSR (even more):
              int8_t  _tap1 = get_tap1(); 
              int8_t  _tap2 = get_tap2(); 

              if (get_tap1_cv_source())
                _tap1 += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_tap1_cv_source() - 1)) + 64) >> 6;             
               
              if (get_tap2_cv_source())
                _tap2 += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_tap2_cv_source() - 1)) + 64) >> 6;

              CONSTRAIN(_tap1, 1, _length);
              CONSTRAIN(_tap2, 1, _length);
     
              _tap1 = (_shift_register >> _tap1) & 1u;  // bit at tap1
              _tap2 = (_shift_register >> _tap2) & 1u;  // bit at tap1
            
              _shift_register = (_shift_register >> 1) | ((_myfirstbit ^ _tap1 ^ _tap2) << (_length - 1)); 
              turing_machine_.set_shift_register(_shift_register);
            }
            break;
          case RANDOM: {
               // get threshold setting:
               int16_t _n = rand_n();   
               if (get_rand_n_cv_source()) {
                 _n += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_rand_n_cv_source() - 1)) + 64) >> 6;             
                 CONSTRAIN(_n, 0, RND_MAX);
               }
               
               int16_t _rand_new = random(RND_MAX);
               _out = _rand_new > _n ? ON : OFF; // DAC needs special care ... 
            }
            break;
          case EUCLID: {
              // simple euclidian algorithm, found on pd hurleur:
              int16_t _n, _k, _offset;
              // the three parameters:
              _n = euclid_n();
              _k = euclid_k();
              _offset = euclid_offset();
              // CV -- 
              if (get_euclid_n_cv_source()) {
                _n += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_euclid_n_cv_source() - 1)) + 64) >> 6;   
                CONSTRAIN(_n, 1, EUCLID_N_MAX);
              }

              if (get_euclid_k_cv_source()) 
                _k += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_euclid_k_cv_source() - 1)) + 64) >> 6;   
             
              if (get_euclid_offset_cv_source()) 
                _offset += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_euclid_offset_cv_source() - 1)) + 64) >> 6;   
          
              CONSTRAIN(_k, 1, _n);
              CONSTRAIN(_offset, 1, _n);
              // calculate output:  
              _out = ((clk_cnt_ + _offset) * _k) % _n;
              _out = (_out < _k) ? ON : OFF;
            }
            break;
          case SEQ: {
              // sequencer mode, adapted from o_C scale edit:
              int16_t _seq = get_sequence();
              
              if (get_sequence_cv_source()) {
                _seq += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_sequence_cv_source() - 1)) + 255) >> 9;             
                CONSTRAIN(_seq, 0, TU::Patterns::PATTERN_USER_LAST-1); 
              }


              uint8_t _playmode = get_playmode();
              
              if (_playmode) {

                // concatenate sequences:
                if (_playmode <= 3 && clk_cnt_ >= get_sequence_length(sequence_last_)) {
                  sequence_cnt_++;
                  sequence_last_ = _seq + (sequence_cnt_ % (_playmode+1));
                  clk_cnt_ = 0; 
                }
                else if (_playmode > 3 && sequence_advance_) {
                  _playmode -= 3;
                  sequence_cnt_++;
                  sequence_last_ = _seq + (sequence_cnt_ % (_playmode+1));
                  clk_cnt_ = 0;
                  sequence_advance_ = false; 
                }
               
                if (sequence_last_ >= TU::Patterns::PATTERN_USER_LAST)
                  sequence_last_ -= TU::Patterns::PATTERN_USER_LAST;
              }
              else 
                sequence_last_ = _seq;
                         
              _seq = sequence_last_;
              // this is the sequence # (USER1-USER4):
              display_sequence_ = _seq;
              // and corresponding pattern mask:
              uint16_t _mask = get_mask(_seq);
              // rotate mask ?
              if (get_mask_cv_source())
                _mask = update_sequence((TU::ADC::value(static_cast<ADC_CHANNEL>(get_mask_cv_source() - 1)) + 127) >> 8, _seq, _mask);
              display_mask_ = _mask;
              // reset counter ?
              if (clk_cnt_ >= get_sequence_length(_seq))
                clk_cnt_ = 0; 
              // output slot at current position:  
              _out = (_mask >> clk_cnt_) & 1u;
              _out = _out ? ON : OFF;
            }   
            break; 
          case DAC: {
               // only available in channel 4: 
               int16_t _range = dac_range(); // 1-255
               int8_t _mode = dac_mode(); 
               
               if (get_DAC_mode_cv_source()) {
                _mode += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_DAC_mode_cv_source() - 1)) + 256) >> 9;             
                CONSTRAIN(_mode, 0, LAST_DACMODE-1);
               }

               if (get_DAC_range_cv_source()) {
                _range += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_DAC_range_cv_source() - 1)) + 16) >> 4;
                CONSTRAIN(_range, 1, 255);              
               }

               switch (_mode) {

                  case _BINARY: {
  
                     uint8_t _binary = 0;
                     // MSB .. LSB
                     if (binary_tracking()) {
                       _binary |= (TU::OUTPUTS::state(CLOCK_CHANNEL_1) & 1u) << 4;
                       _binary |= (TU::OUTPUTS::state(CLOCK_CHANNEL_2) & 1u) << 3;
                       _binary |= (TU::OUTPUTS::state(CLOCK_CHANNEL_3) & 1u) << 2;
                       _binary |= (TU::OUTPUTS::state(CLOCK_CHANNEL_5) & 1u) << 1;
                       _binary |= (TU::OUTPUTS::state(CLOCK_CHANNEL_6) & 1u);
                     }
                     else {
                       _binary |= (TU::OUTPUTS::value(CLOCK_CHANNEL_1) & 1u) << 4;
                       _binary |= (TU::OUTPUTS::value(CLOCK_CHANNEL_2) & 1u) << 3;
                       _binary |= (TU::OUTPUTS::value(CLOCK_CHANNEL_3) & 1u) << 2;
                       _binary |= (TU::OUTPUTS::value(CLOCK_CHANNEL_5) & 1u) << 1;
                       _binary |= (TU::OUTPUTS::value(CLOCK_CHANNEL_6) & 1u);
                     }
                     ++_binary; // 32 max
                     
                     int16_t _dac_code = (static_cast<int16_t>(_binary) << 7) - 0x800; // +/- 2048
                     _dac_code = signed_multiply_32x16b((static_cast<int32_t>(_range) * 65535U) >> 8, _dac_code);
                     _dac_code = signed_saturate_rshift(_dac_code, 16, 0);
                     
                     _out = _ZERO - _dac_code;
                   }
                   break;
                  case _RANDOM: {
  
                     uint16_t _history[TU::OUTPUTS::kHistoryDepth];
                     int16_t _rand_history, _rand_new, _depth, _weight;

                     _depth  = get_history_depth();
                     _weight = get_history_weight();

                     if (get_rand_history_d_cv_source()) {
                         _depth += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_rand_history_d_cv_source() - 1)) + 256) >> 9;             
                         CONSTRAIN(_depth, 0, (int8_t) TU::OUTPUTS::kHistoryDepth - 1 );
                     }
                     if (get_rand_history_w_cv_source()) {
                        _weight += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_rand_history_w_cv_source() - 1)) + 16) >> 4;
                        CONSTRAIN(_weight, 0, 255);
                     }
  
                     TU::OUTPUTS::getHistory<CLOCK_CHANNEL_4>(_history);
                     
                     _rand_history = calc_average(_history,_depth);     
                     _rand_history = signed_multiply_32x16b((static_cast<int32_t>(_weight) * 65535U) >> 8, _rand_history);
                     _rand_history = signed_saturate_rshift(_rand_history, 16, 0);
                     
                     _rand_new = random(0xFFF) - 0x800; // +/- 2048
                     _rand_new = signed_multiply_32x16b((static_cast<int32_t>(_range) * 65535U) >> 8, _rand_new);
                     _rand_new = signed_saturate_rshift(_rand_new, 16, 0);
                     _out = _ZERO - (_rand_new + _rand_history);
                   }
                   break;
                  case _TURING: {
                  
                     int16_t _length = get_turing_length();
                     int16_t _probability = get_turing_probability();

                     if (get_turing_length_cv_source()) {
                        _length += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_turing_length_cv_source() - 1)) + 64) >> 7;   
                        CONSTRAIN(_length, LFSR_MIN, LFSR_MAX);
                     }

                     if (get_turing_prob_cv_source()) {
                        _probability += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_turing_prob_cv_source() - 1)) + 16) >> 4;
                        CONSTRAIN(_probability, 1, 255);              
                     }
                  
                     turing_machine_.set_length(_length);
                     turing_machine_.set_probability(_probability); 
                  
                     int32_t _shift_register = (static_cast<int16_t>(turing_machine_.Clock()) & 0xFFF); //
                     // return useful values for small values of _length:
                     if (_length < 12) {
                      _shift_register = _shift_register << (12 -_length);
                      _shift_register = _shift_register & 0xFFF;
                     }
                     _shift_register -= 0x800; // +/- 2048
                     _shift_register = signed_multiply_32x16b((static_cast<int32_t>(_range) * 65535U) >> 8, _shift_register);
                     _shift_register = signed_saturate_rshift(_shift_register, 16, 0);
                     _out = _ZERO - _shift_register;
                   }
                   break;
                  case _LOGISTIC: {
                     logistic_map_.set_seed(123);
                     
                     int32_t logistic_map_r = get_logistic_map_r();
                     
                     if (get_logistic_map_r_cv_source()) {
                        logistic_map_r += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_logistic_map_r_cv_source() - 1)) + 16) >> 4;
                        CONSTRAIN(logistic_map_r, 0, 255);
                     } 
     
                     logistic_map_.set_r(logistic_map_r);
                     
                     int16_t _logistic_map_x = (static_cast<int16_t>(logistic_map_.Clock()) & 0xFFF) - 0x800; // +/- 2048
                     _logistic_map_x = signed_multiply_32x16b((static_cast<int32_t>(_range) * 65535U) >> 8, _logistic_map_x);
                     _logistic_map_x = signed_saturate_rshift(_logistic_map_x, 16, 0);
                     _out = _ZERO - _logistic_map_x;
                  }
                  break;
                 default:
                  break;   
               }   // end DAC mode switch 
            }
            break;
           default:
            break; // end mode switch       
      }
      return _out; 
  }

  inline void logic(CLOCK_CHANNEL clock_channel) {

     if (!logic_)
       return;
     logic_ = false;  

     // else logic ... 
     uint16_t _out = OFF;
     int8_t _op1, _op2, _type;
     
     _type = logic_type();
     _op1  = logic_op1();
     _op2  = logic_op2();

     if (get_logic_type_cv_source()) {
        _type += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_logic_type_cv_source() - 1)) + 256) >> 9;             
        CONSTRAIN(_type, 0, LOGICMODE_LAST-1);
     }

     if (get_logic_op1_cv_source())  {
        _op1 += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_logic_op1_cv_source() - 1)) + 127) >> 8;             
        CONSTRAIN(_op1, 0, NUM_CHANNELS-1);
     }
     
     if (get_logic_op2_cv_source()) {
        _op2 += (TU::ADC::value(static_cast<ADC_CHANNEL>(get_logic_op2_cv_source() - 1)) + 127) >> 8;             
        CONSTRAIN(_op2, 0, NUM_CHANNELS-1);
     }

     // this doesn't care if CHANNEL_4 is in DAC mode (= mostly always true); but so what.
     if (!logic_tracking()) {
       _op1 = TU::OUTPUTS::state(_op1) & 1u;
       _op2 = TU::OUTPUTS::state(_op2) & 1u;
     }
     else {
       _op1 = TU::OUTPUTS::value(_op1) & 1u;
       _op2 = TU::OUTPUTS::value(_op2) & 1u;
     }

     // and go through the options ...
     switch (_type) {
  
        case AND:  
            _out = _op1 & _op2;
            break;
        case OR:   
            _out = _op1 | _op2;
            break;
        case XOR:  
            _out = _op1 ^ _op2;
            break;
        case XNOR:  
            _out = ~(_op1 ^ _op2);
            break;
        case NAND: 
            _out = ~(_op1 & _op2);
            break;
        case NOR:  
            _out = ~(_op1 | _op2);
            break;
        default: 
            break;    
    } // end logic op switch

    // write to output:
    gpio_state_ = _out = (_out & 1u) ? ON : OFF;
    if (!_out && clock_channel == CLOCK_CHANNEL_4)
      _out += _ZERO;
    TU::OUTPUTS::setState(clock_channel, _out); 
    TU::OUTPUTS::set(clock_channel, _out);  
  }

  inline uint16_t calc_average(const uint16_t *data, uint8_t depth) {
    uint32_t sum = 0;
    uint8_t n = depth;
    while (n--)
      sum += *data++;
    return sum / depth;
  }
  
 
  ChannelSetting enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  void update_enabled_settings(uint8_t channel_id) {

    
    ChannelSetting *settings = enabled_settings_;
    uint8_t mode = (channel_id != CLOCK_CHANNEL_4) ? get_mode() : get_mode4();

    if (menu_page_ != TEMPO) {
      
      if (channel_id != CLOCK_CHANNEL_4)
        *settings++ = CHANNEL_SETTING_MODE;
      else   
        *settings++ = CHANNEL_SETTING_MODE4;
    }
    else 
      *settings++ = CHANNEL_SETTING_INTERNAL_CLK;
          

    if (menu_page_ == CV_SOURCES) {

      if (mode != DAC)
        *settings++ = CHANNEL_SETTING_PULSEWIDTH_CV_SOURCE;
      *settings++ = CHANNEL_SETTING_MULT_CV_SOURCE;
     
      switch (mode) {

          case LFSR:
            *settings++ = CHANNEL_SETTING_TURING_LENGTH_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_TURING_PROB_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_LFSR_TAP1_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_LFSR_TAP2_CV_SOURCE;
            break;
          case EUCLID:
            *settings++ = CHANNEL_SETTING_EUCLID_N_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_EUCLID_K_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_EUCLID_OFFSET_CV_SOURCE;
            break; 
          case RANDOM:
            *settings++ = CHANNEL_SETTING_RAND_N_CV_SOURCE;
            break;
          case LOGIC:
            *settings++ = CHANNEL_SETTING_LOGIC_TYPE_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_LOGIC_OP1_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_LOGIC_OP2_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_DUMMY;
            break; 
          case SEQ:
            *settings++ = CHANNEL_SETTING_SEQ_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_MASK_CV_SOURCE;
            *settings++ = CHANNEL_SETTING_DUMMY; // playmode CV
            break;
          case DAC: 
            *settings++ = CHANNEL_SETTING_DAC_MODE_CV_SOURCE; 
            *settings++ = CHANNEL_SETTING_DAC_RANGE_CV_SOURCE;
            switch (dac_mode())  {
              case _BINARY:
                *settings++ = CHANNEL_SETTING_DUMMY;
                break;
              case _RANDOM:
                *settings++ = CHANNEL_SETTING_HISTORY_WEIGHT_CV_SOURCE;
                *settings++ = CHANNEL_SETTING_HISTORY_DEPTH_CV_SOURCE;
                break;
              case _TURING:
                *settings++ = CHANNEL_SETTING_TURING_LENGTH_CV_SOURCE;
                *settings++ = CHANNEL_SETTING_TURING_PROB_CV_SOURCE;
                break;
              case _LOGISTIC:
                *settings++ = CHANNEL_SETTING_LOGISTIC_MAP_R_CV_SOURCE;
                break;
              default:
                break;                
            }
          default:
            break;
      }
      *settings++ = CHANNEL_SETTING_CLOCK_CV_SOURCE;
      //if (mode == SEQ || mode == EUCLID) 
      *settings++ = CHANNEL_SETTING_DUMMY; // make # items the same / no CV for reset source ...
       
    }

    else if (menu_page_ == PARAMETERS) {

        if (mode != DAC)
          *settings++ = CHANNEL_SETTING_PULSEWIDTH;
        *settings++ = CHANNEL_SETTING_MULT;
    
        switch (mode) {
    
          case LFSR: 
           *settings++ = CHANNEL_SETTING_TURING_LENGTH;
           *settings++ = CHANNEL_SETTING_TURING_PROB;
           *settings++ = CHANNEL_SETTING_LFSR_TAP1;
           *settings++ = CHANNEL_SETTING_LFSR_TAP2;
           break;
          case EUCLID:
           *settings++ = CHANNEL_SETTING_EUCLID_N;
           *settings++ = CHANNEL_SETTING_EUCLID_K;
           *settings++ = CHANNEL_SETTING_EUCLID_OFFSET;
           break; 
          case RANDOM:
           *settings++ = CHANNEL_SETTING_RAND_N;
           break;
          case LOGIC:
           *settings++ = CHANNEL_SETTING_LOGIC_TYPE;
           *settings++ = CHANNEL_SETTING_LOGIC_OP1;
           *settings++ = CHANNEL_SETTING_LOGIC_OP2;
           *settings++ = CHANNEL_SETTING_LOGIC_TRACK_WHAT;
           break; 
          case SEQ:
           *settings++ = CHANNEL_SETTING_SEQUENCE;
           
           switch (get_sequence()) {
              case 0:
              *settings++ = CHANNEL_SETTING_MASK1;
              break;
              case 1:
              *settings++ = CHANNEL_SETTING_MASK2;
              break;
              case 2:
              *settings++ = CHANNEL_SETTING_MASK3;
              break;
              case 3:
              *settings++ = CHANNEL_SETTING_MASK4;
              break;
              default:
              break;
           }
            *settings++ = CHANNEL_SETTING_SEQUENCE_PLAYMODE;
           break;
          case DAC: 
            *settings++ = CHANNEL_SETTING_DAC_MODE; 
            *settings++ = CHANNEL_SETTING_DAC_RANGE;
            switch (dac_mode())  {
    
              case _BINARY:
                *settings++ = CHANNEL_SETTING_DAC_TRACK_WHAT;
                break;
              case _RANDOM:
                *settings++ = CHANNEL_SETTING_HISTORY_WEIGHT;
                *settings++ = CHANNEL_SETTING_HISTORY_DEPTH;
                break;
              case _TURING:
                *settings++ = CHANNEL_SETTING_TURING_LENGTH;
                *settings++ = CHANNEL_SETTING_TURING_PROB;
                break;
              case _LOGISTIC:
                *settings++ = CHANNEL_SETTING_LOGISTIC_MAP_R;
                break;
              default:
                break;                
            }
           break;  
          default:
           break;
        } // end mode switch
    
        *settings++ = CHANNEL_SETTING_CLOCK;
        *settings++ = CHANNEL_SETTING_RESET;  
    }
    else if (menu_page_ == TEMPO) {
      
      *settings++ = CHANNEL_SETTING_DUMMY_EMPTY;
      *settings++ = CHANNEL_SETTING_SCREENSAVER;
      *settings++ = CHANNEL_SETTING_DUMMY_EMPTY;
    }
    num_enabled_settings_ = settings - enabled_settings_;  
  }
  

  uint16_t update_sequence(int32_t mask_rotate_, uint8_t sequence_, uint16_t mask_) {
    
    const int sequence_num = sequence_;
    uint16_t mask = mask_;
    
    if (mask_rotate_)
      mask = TU::PatternEditor<Clock_channel>::RotateMask(mask, get_sequence_length(sequence_num), mask_rotate_);
    return mask;
  }

  void RenderScreensaver(weegfx::coord_t start_x, CLOCK_CHANNEL clock_channel) const;
  
private:
  uint16_t _sync_cnt;
  bool force_update_;
  uint16_t _ZERO;
  uint8_t clk_src_;
  bool reset_;
  bool reset_me_;
  bool reset_counter_;
  uint32_t ticks_;
  uint32_t subticks_;
  uint32_t tickjitter_;
  uint32_t clk_cnt_;
  int16_t div_cnt_;
  uint32_t ext_frequency_in_ticks_;
  uint32_t channel_frequency_in_ticks_;
  uint32_t pulse_width_in_ticks_;
  uint16_t gpio_state_;
  uint8_t prev_multiplier_;
  uint8_t prev_pulsewidth_;
  uint8_t logic_;
  uint8_t display_sequence_;
  uint16_t display_mask_;
  int8_t sequence_last_;
  int32_t sequence_cnt_;
  int8_t sequence_advance_;
  int8_t sequence_advance_state_;
  uint8_t menu_page_;
  uint16_t bpm_last_;
 
  util::TuringShiftRegister turing_machine_;
  util::LogisticMap logistic_map_;
  int num_enabled_settings_;
  ChannelSetting enabled_settings_[CHANNEL_SETTING_LAST];
  TU::DigitalInputDisplay clock_display_;

};

const char* const channel_trigger_sources[CHANNEL_TRIGGER_LAST] = {
  "TR1", "TR2", "none", "INT"
};

const char* const reset_trigger_sources[CHANNEL_TRIGGER_LAST+1] = {
  "RST1", "RST2", "none", "=HI2", "=LO2"
};

const char* const multipliers[] = {
  "/64", "/48", "/32", "/16", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "-", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "QN24", "QN96"
};

const char* const cv_sources[5] = {
  "--", "CV1", "CV2", "CV3", "CV4"
};

SETTINGS_DECLARE(Clock_channel, CHANNEL_SETTING_LAST) {
 
  { 0, 0, MODES - 2, "mode", TU::Strings::mode, settings::STORAGE_TYPE_U4 },
  { 0, 0, MODES - 1, "mode", TU::Strings::mode, settings::STORAGE_TYPE_U4 },
  { CHANNEL_TRIGGER_TR1, 0, CHANNEL_TRIGGER_LAST-1, "clock src", channel_trigger_sources, settings::STORAGE_TYPE_U4 },
  { CHANNEL_TRIGGER_NONE, 0, CHANNEL_TRIGGER_LAST, "reset/mute", reset_trigger_sources, settings::STORAGE_TYPE_U4 },
  { MULT_BY_ONE, 0, MULT_MAX, "mult/div", multipliers, settings::STORAGE_TYPE_U8 },
  { 25, 0, PULSEW_MAX, "pulsewidth", TU::Strings::pulsewidth_ms, settings::STORAGE_TYPE_U8 },
  { 100, BPM_MIN, BPM_MAX, "BPM:", NULL, settings::STORAGE_TYPE_U16 },
  //
  { 0, 0, 31, "LFSR tap1",NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, 31, "LFSR tap2",NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, RND_MAX, "rand > n", NULL, settings::STORAGE_TYPE_U8 },
  { 3, 3, EUCLID_N_MAX, "euclid: N", NULL, settings::STORAGE_TYPE_U8 },
  { 1, 1, EUCLID_N_MAX, "euclid: K", NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, EUCLID_N_MAX-1, "euclid: OFFSET", NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, LOGICMODE_LAST-1, "logic type", TU::Strings::operators, settings::STORAGE_TYPE_U8 },
  { 0, 0, NUM_CHANNELS - 1, "op_1", TU::Strings::channel_id, settings::STORAGE_TYPE_U8 },
  { 1, 0, NUM_CHANNELS - 1, "op_2", TU::Strings::channel_id, settings::STORAGE_TYPE_U8 },
  { 0, 0, 1, "track -->", TU::Strings::logic_tracking, settings::STORAGE_TYPE_U4 },
  { 128, 1, 255, "DAC: range", NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, DAC_MODES-1, "DAC: mode", TU::Strings::dac_modes, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "track -->", TU::Strings::binary_tracking, settings::STORAGE_TYPE_U4 },
  { 0, 0, 255, "rnd hist.", NULL, settings::STORAGE_TYPE_U8 }, /// "history"
  { 0, 0, TU::OUTPUTS::kHistoryDepth - 1, "hist. depth", NULL, settings::STORAGE_TYPE_U8 }, /// "history"
  { LFSR_MIN<<1, LFSR_MIN, LFSR_MAX, "LFSR length", NULL, settings::STORAGE_TYPE_U8 },
  { 128, 0, 255, "LFSR p(x)", NULL, settings::STORAGE_TYPE_U8 },
  { 1, 1, 255, "LGST(R)", NULL, settings::STORAGE_TYPE_U8 },
  { 65535, 0, 65535, "--> edit", NULL, settings::STORAGE_TYPE_U16 }, // seq 1
  { 65535, 0, 65535, "--> edit", NULL, settings::STORAGE_TYPE_U16 }, // seq 2
  { 65535, 0, 65535, "--> edit", NULL, settings::STORAGE_TYPE_U16 }, // seq 3
  { 65535, 0, 65535, "--> edit", NULL, settings::STORAGE_TYPE_U16 }, // seq 4
  { TU::Patterns::PATTERN_USER_0, 0, TU::Patterns::PATTERN_USER_LAST-1, "sequence #", TU::pattern_names_short, settings::STORAGE_TYPE_U8 },
  { TU::Patterns::kMax, TU::Patterns::kMin, TU::Patterns::kMax, "sequence length", NULL, settings::STORAGE_TYPE_U8 }, // seq 1
  { TU::Patterns::kMax, TU::Patterns::kMin, TU::Patterns::kMax, "sequence length", NULL, settings::STORAGE_TYPE_U8 }, // seq 2
  { TU::Patterns::kMax, TU::Patterns::kMin, TU::Patterns::kMax, "sequence length", NULL, settings::STORAGE_TYPE_U8 }, // seq 3
  { TU::Patterns::kMax, TU::Patterns::kMin, TU::Patterns::kMax, "sequence length", NULL, settings::STORAGE_TYPE_U8 }, // seq 4
  { 0, 0, 6, "playmode", TU::Strings::seq_playmodes, settings::STORAGE_TYPE_U4 },
  // cv sources
  { 0, 0, 4, "mult/div    >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "pulsewidth  >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "clock src   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "LFSR tap1   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "LFSR tap2   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "rand > n    >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "euclid: N   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "euclid: K   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "euclid: OFF >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "logic type  >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "op_1        >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "op_2        >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "LFSR p(x)   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "LFSR length >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "LGST(R)     >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "sequence #  >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "mask        >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "DAC: range  >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "DAC: mode   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "rnd hist.   >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 4, "hist. depth >>", cv_sources, settings::STORAGE_TYPE_U4 },
  { 0, 0, 0, "---------------------", NULL, settings::STORAGE_TYPE_U4 }, // DUMMY
  { 0, 0, 0, "  ", NULL, settings::STORAGE_TYPE_U4 }, // DUMMY empty
  { 0, 0, 0, "  ", NULL, settings::STORAGE_TYPE_U4 }  // screensaver
};


class ClocksState {
public:
  void Init() {
    selected_channel = 0;
    cursor.Init(CHANNEL_SETTING_MODE, CHANNEL_SETTING_LAST - 1);
    pattern_editor.Init();
  }

  inline bool editing() const {
    return cursor.editing();
  }

  inline int cursor_pos() const {
    return cursor.cursor_pos();
  }

  int selected_channel;
  menu::ScreenCursor<menu::kScreenLines> cursor;
  menu::ScreenCursor<menu::kScreenLines> cursor_state;
  TU::PatternEditor<Clock_channel> pattern_editor;
};

ClocksState clocks_state;
Clock_channel clock_channel[NUM_CHANNELS];

void CLOCKS_init() {

  TU::Patterns::Init();

  ext_frequency[CHANNEL_TRIGGER_TR1]  = 0xFFFFFFFF;
  ext_frequency[CHANNEL_TRIGGER_TR2]  = 0xFFFFFFFF;
  ext_frequency[CHANNEL_TRIGGER_NONE] = 0xFFFFFFFF;

  clocks_state.Init();
  for (size_t i = 0; i < NUM_CHANNELS; ++i) 
    clock_channel[i].Init(static_cast<ChannelTriggerSource>(CHANNEL_TRIGGER_TR1));
  clocks_state.cursor.AdjustEnd(clock_channel[0].num_enabled_settings() - 1);
}

size_t CLOCKS_storageSize() {
  return NUM_CHANNELS * Clock_channel::storageSize();
}

size_t CLOCKS_save(void *storage) {

  size_t used = 0;
  for (size_t i = 0; i < NUM_CHANNELS; ++i) {
    used += clock_channel[i].Save(static_cast<char*>(storage) + used);
  }
  return used;
}
size_t CLOCKS_restore(const void *storage) {
  size_t used = 0;
  for (size_t i = 0; i < NUM_CHANNELS; ++i) {
    used += clock_channel[i].Restore(static_cast<const char*>(storage) + used);
    clock_channel[i].update_enabled_settings(i);
  }
  clocks_state.cursor.AdjustEnd(clock_channel[0].num_enabled_settings() - 1);
  return used;
}

void CLOCKS_handleAppEvent(TU::AppEvent event) {
  switch (event) {
    case TU::APP_EVENT_RESUME:
        clocks_state.cursor.set_editing(false);
        clocks_state.pattern_editor.Close();
    break;
    case TU::APP_EVENT_SUSPEND:
    case TU::APP_EVENT_SCREENSAVER_ON:
    case TU::APP_EVENT_SCREENSAVER_OFF:
    {
      Clock_channel &selected = clock_channel[clocks_state.selected_channel];
        if (selected.get_page() > PARAMETERS) {
          selected.set_page(PARAMETERS);
          selected.update_enabled_settings(clocks_state.selected_channel);
          clocks_state.cursor.Init(CHANNEL_SETTING_MODE, 0); 
          clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1);
        }
    }
    break;
  }
}

void CLOCKS_loop() {
}

void CLOCKS_isr() {

  ticks_src1++; // src #1 ticks
  ticks_src2++; // src #2 ticks

  // resync internal timers?
  if (RESYNC) {
    clock_channel[0].reset_ticks_internal();
    clock_channel[1].reset_ticks_internal();
    clock_channel[2].reset_ticks_internal();
    clock_channel[3].reset_ticks_internal();
    clock_channel[4].reset_ticks_internal();
    clock_channel[5].reset_ticks_internal();
    RESYNC = false;
  }
    
  uint32_t triggers = TU::DigitalInputs::clocked();  

  // clocked? reset ; better use ARM_DWT_CYCCNT ?
  if (triggers == 1)  {
    ext_frequency[CHANNEL_TRIGGER_TR1] = ticks_src1;
    ticks_src1 = 0x0;
  }
  if (triggers == 2) {
    ext_frequency[CHANNEL_TRIGGER_TR2] = ticks_src2;
    ticks_src2 = 0x0;
  }

  // update channels:
  clock_channel[0].Update(triggers, CLOCK_CHANNEL_1);
  clock_channel[1].Update(triggers, CLOCK_CHANNEL_2);
  clock_channel[2].Update(triggers, CLOCK_CHANNEL_3);
  clock_channel[4].Update(triggers, CLOCK_CHANNEL_5);
  clock_channel[5].Update(triggers, CLOCK_CHANNEL_6);
  clock_channel[3].Update(triggers, CLOCK_CHANNEL_4); // = DAC channel; update last, because of the binary Sequencer thing.

  // apply logic ?
  clock_channel[0].logic(CLOCK_CHANNEL_1);
  clock_channel[1].logic(CLOCK_CHANNEL_2);
  clock_channel[2].logic(CLOCK_CHANNEL_3);
  clock_channel[3].logic(CLOCK_CHANNEL_4);
  clock_channel[4].logic(CLOCK_CHANNEL_5);
  clock_channel[5].logic(CLOCK_CHANNEL_6);
}

void CLOCKS_handleButtonEvent(const UI::Event &event) {
  
  if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
     switch (event.control) {
      case TU::CONTROL_BUTTON_UP:
         CLOCKS_upButtonLong();
        break;
      case TU::CONTROL_BUTTON_DOWN:
        CLOCKS_downButtonLong();
        break;
       case TU::CONTROL_BUTTON_L:
        if (!(clocks_state.pattern_editor.active()))
          CLOCKS_leftButtonLong();
        break;  
      default:
        break;
     }
  }
  
  if (clocks_state.pattern_editor.active()) {
    clocks_state.pattern_editor.HandleButtonEvent(event);
    return;
  }
 
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case TU::CONTROL_BUTTON_UP:
        CLOCKS_upButton();
        break;
      case TU::CONTROL_BUTTON_DOWN:
        CLOCKS_downButton();
        break;
      case TU::CONTROL_BUTTON_L:
        CLOCKS_leftButton();
        break;
      case TU::CONTROL_BUTTON_R:
        CLOCKS_rightButton();
        break;
    }
  } 
}

void CLOCKS_handleEncoderEvent(const UI::Event &event) {

  if (clocks_state.pattern_editor.active()) {
    clocks_state.pattern_editor.HandleEncoderEvent(event);
    return;
  }
 
  if (TU::CONTROL_ENCODER_L == event.control) {

    int selected_channel = clocks_state.selected_channel + event.value;
    CONSTRAIN(selected_channel, 0, NUM_CHANNELS-1);
    clocks_state.selected_channel = selected_channel;

    Clock_channel &selected = clock_channel[clocks_state.selected_channel];
    
    if (selected.get_page() == TEMPO || selected.get_page() == CV_SOURCES) 
      selected.set_page(PARAMETERS);
      
    selected.update_enabled_settings(clocks_state.selected_channel);
    clocks_state.cursor.Init(CHANNEL_SETTING_MODE, 0); 
    clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1);
    
  } else if (TU::CONTROL_ENCODER_R == event.control) {
    
       Clock_channel &selected = clock_channel[clocks_state.selected_channel];

       if (selected.get_page() == TEMPO) {

         uint16_t int_clock_used_ = 0x0;
         for (int i = 0; i < NUM_CHANNELS; i++) 
              int_clock_used_ += selected.get_clock_source() == CHANNEL_TRIGGER_INTERNAL ? 0x10 : 0x00;
         if (!int_clock_used_) {
          selected.set_page(PARAMETERS);
          clocks_state.cursor = clocks_state.cursor_state;
          selected.update_enabled_settings(clocks_state.selected_channel);
          return;     
         }
       }
    
       if (clocks_state.editing()) {
        
          ChannelSetting setting = selected.enabled_setting_at(clocks_state.cursor_pos());
          
          if (CHANNEL_SETTING_MASK1 != setting || CHANNEL_SETTING_MASK2 != setting || CHANNEL_SETTING_MASK3 != setting || CHANNEL_SETTING_MASK4 != setting) {
            
            if (selected.change_value(setting, event.value))
             selected.force_update();

            switch (setting) {

              case CHANNEL_SETTING_SEQUENCE:
              {
                if (!selected.get_playmode()) {
                    uint8_t seq = selected.get_sequence();
                    selected.pattern_changed(selected.get_mask(seq));
                    selected.set_display_sequence(seq);
                } 
              }
              break;
              case CHANNEL_SETTING_MODE:
              case CHANNEL_SETTING_MODE4:  
              case CHANNEL_SETTING_DAC_MODE:
                 selected.update_enabled_settings(clocks_state.selected_channel);
                 clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1);
              break;
                   // special cases:
              case CHANNEL_SETTING_EUCLID_N:
              case CHANNEL_SETTING_EUCLID_K: 
              {
                 uint8_t _n = selected.euclid_n();
                 if (selected.euclid_k() > _n)
                    selected.set_euclid_k(_n);
                 if (selected.euclid_offset() > _n)
                    selected.set_euclid_offset(_n);   
              }
              break;
              case CHANNEL_SETTING_EUCLID_OFFSET: 
              {
                 uint8_t _n = selected.euclid_n();
                 if (selected.euclid_offset() > _n)
                    selected.set_euclid_offset(_n);
              }
              break;
              case CHANNEL_SETTING_TURING_LENGTH: 
              {
                 uint8_t _len = selected.get_turing_length();
                 if (selected.get_tap1() > _len)
                    selected.set_tap1(_len);
                 if (selected.get_tap2() > _len)
                    selected.set_tap2(_len);   
              }
              break;
              case CHANNEL_SETTING_LFSR_TAP1: 
              {
                 uint8_t _len = selected.get_turing_length();
                 if (selected.get_tap1() > _len)
                    selected.set_tap1(_len);
              }
              break;
              case CHANNEL_SETTING_LFSR_TAP2: 
              {
                 uint8_t _len = selected.get_turing_length();
                 if (selected.get_tap2() > _len)
                    selected.set_tap2(_len);
              }
              break;  
             default:
              break;
            }
        }
      } else {
      clocks_state.cursor.Scroll(event.value);
    }
  }
}

void CLOCKS_upButton() {

  Clock_channel &selected = clock_channel[clocks_state.selected_channel];

  uint8_t _menu_page = selected.get_page();

  switch (_menu_page) {

    case TEMPO:
      selected.set_page(PARAMETERS);
      clocks_state.cursor = clocks_state.cursor_state;
      break;
    default:  
      clocks_state.cursor_state = clocks_state.cursor;
      selected.set_page(TEMPO);
      clocks_state.cursor.Init(CHANNEL_SETTING_MODE, 0);
      clocks_state.cursor.toggle_editing();
     break;
  }
  selected.update_enabled_settings(clocks_state.selected_channel); 
  clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1); 
}

void CLOCKS_downButton() {

  Clock_channel &selected = clock_channel[clocks_state.selected_channel];

  uint8_t _menu_page = selected.get_page();

  switch (_menu_page) {

    case CV_SOURCES:
      // don't get stuck:
      if (selected.enabled_setting_at(clocks_state.cursor_pos()) == CHANNEL_SETTING_MASK_CV_SOURCE)
        clocks_state.cursor.set_editing(false);
      selected.set_page(PARAMETERS);
      break;
    case TEMPO:
      selected.set_page(CV_SOURCES);
      clocks_state.cursor = clocks_state.cursor_state;  
    default:  
      // don't get stuck: 
      if (selected.enabled_setting_at(clocks_state.cursor_pos()) == CHANNEL_SETTING_RESET)
        clocks_state.cursor.set_editing(false);
      selected.set_page(CV_SOURCES);
     break;
  }
  selected.update_enabled_settings(clocks_state.selected_channel); 
  clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1);
}

void CLOCKS_rightButton() {

  Clock_channel &selected = clock_channel[clocks_state.selected_channel];

  if (selected.get_page() == TEMPO) {
    selected.set_page(PARAMETERS);
    clocks_state.cursor = clocks_state.cursor_state;
    selected.update_enabled_settings(clocks_state.selected_channel);
    clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1); 
    return;
  }
  
  switch (selected.enabled_setting_at(clocks_state.cursor_pos())) {

    case CHANNEL_SETTING_MASK1:
    case CHANNEL_SETTING_MASK2:
    case CHANNEL_SETTING_MASK3:
    case CHANNEL_SETTING_MASK4:
    {
      int pattern = selected.get_sequence();
      if (TU::Patterns::PATTERN_NONE != pattern) {
        clocks_state.pattern_editor.Edit(&selected, pattern);
      }
    }
    break;
    case CHANNEL_SETTING_DUMMY:
    case CHANNEL_SETTING_DUMMY_EMPTY:
    break; 
    default:
     clocks_state.cursor.toggle_editing();
    break;
  }

}

void CLOCKS_leftButton() {
 
  Clock_channel &selected = clock_channel[clocks_state.selected_channel];

  if (selected.get_page() == TEMPO) {
    selected.set_page(PARAMETERS);
    clocks_state.cursor = clocks_state.cursor_state;
    selected.update_enabled_settings(clocks_state.selected_channel);
    clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1); 
    return;
  }
  // sync:
  for (int i = 0; i < NUM_CHANNELS; ++i) 
        clock_channel[i].sync();
}

void CLOCKS_leftButtonLong() {
    
  for (int i = 0; i < NUM_CHANNELS; ++i) 
        clock_channel[i].InitDefaults(); 
    
  Clock_channel &selected = clock_channel[clocks_state.selected_channel];
  selected.set_page(PARAMETERS);
  selected.update_enabled_settings(clocks_state.selected_channel);
  clocks_state.cursor.set_editing(false);
  clocks_state.cursor.AdjustEnd(selected.num_enabled_settings() - 1); 
}

void CLOCKS_upButtonLong() {

  Clock_channel &selected = clock_channel[clocks_state.selected_channel];
  // set all channels to internal ?
  if (selected.get_page() == TEMPO) {
    for (int i = 0; i < NUM_CHANNELS; ++i) 
        clock_channel[i].set_clock_source(CHANNEL_TRIGGER_INTERNAL);
    // and clear outputs:
    TU::OUTPUTS::set(CLOCK_CHANNEL_1, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_1, OFF);
    TU::OUTPUTS::set(CLOCK_CHANNEL_2, OFF); 
    TU::OUTPUTS::setState(CLOCK_CHANNEL_2, OFF);
    TU::OUTPUTS::set(CLOCK_CHANNEL_3, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_3, OFF); 
    TU::OUTPUTS::set(CLOCK_CHANNEL_4, selected.get_zero(CLOCK_CHANNEL_4)); 
    TU::OUTPUTS::setState(CLOCK_CHANNEL_4, OFF);
    TU::OUTPUTS::set(CLOCK_CHANNEL_5, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_5, OFF);
    TU::OUTPUTS::set(CLOCK_CHANNEL_6, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_6, OFF);      
  }
}

void CLOCKS_downButtonLong() {

  Clock_channel &selected = clock_channel[clocks_state.selected_channel];
  
  if (selected.get_page() == CV_SOURCES)
    selected.clear_CV_mapping();
  else if (selected.get_page() == TEMPO)   {
    for (int i = 0; i < NUM_CHANNELS; ++i)
        clock_channel[i].set_clock_source(CHANNEL_TRIGGER_TR1);
    // and clear outputs:
    TU::OUTPUTS::set(CLOCK_CHANNEL_1, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_1, OFF);
    TU::OUTPUTS::set(CLOCK_CHANNEL_2, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_2, OFF);  
    TU::OUTPUTS::set(CLOCK_CHANNEL_3, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_3, OFF);  
    TU::OUTPUTS::set(CLOCK_CHANNEL_4, selected.get_zero(CLOCK_CHANNEL_4)); 
    TU::OUTPUTS::setState(CLOCK_CHANNEL_4, OFF);  
    TU::OUTPUTS::set(CLOCK_CHANNEL_5, OFF);
    TU::OUTPUTS::setState(CLOCK_CHANNEL_5, OFF); 
    TU::OUTPUTS::set(CLOCK_CHANNEL_6, OFF);   
    TU::OUTPUTS::setState(CLOCK_CHANNEL_6, OFF);     
  }
}

void CLOCKS_menu() {

  menu::SixTitleBar::Draw();
  uint16_t int_clock_used_ = 0x0;
  
  for (int i = 0, x = 0; i < NUM_CHANNELS; ++i, x += 21) {

    const Clock_channel &channel = clock_channel[i];
    menu::SixTitleBar::SetColumn(i);
    graphics.print((char)('1' + i));
    graphics.movePrintPos(2, 0);
    //
    uint16_t internal_ = channel.get_clock_source() == CHANNEL_TRIGGER_INTERNAL ? 0x10 : 0x00;
    int_clock_used_ += internal_;
    menu::SixTitleBar::DrawGateIndicator(i, internal_);
  }
  
  const Clock_channel &channel = clock_channel[clocks_state.selected_channel];
  if (channel.get_page() != TEMPO)
    menu::SixTitleBar::Selected(clocks_state.selected_channel);

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(clocks_state.cursor);
  
  menu::SettingsListItem list_item;

   while (settings_list.available()) {
    const int setting =
        channel.enabled_setting_at(settings_list.Next(list_item));
    const int value = channel.get_value(setting);
    const settings::value_attr &attr = Clock_channel::value_attr(setting);

    switch (setting) {

      case CHANNEL_SETTING_MASK1:
      case CHANNEL_SETTING_MASK2:
      case CHANNEL_SETTING_MASK3:
      case CHANNEL_SETTING_MASK4:
        menu::DrawMask<false, 16, 8, 1>(menu::kDisplayWidth, list_item.y, channel.get_display_mask(), channel.get_sequence_length(channel.get_display_sequence()), channel.get_clock_cnt());
        list_item.DrawNoValue<false>(value, attr);
        break;
      case CHANNEL_SETTING_DUMMY:
      case CHANNEL_SETTING_DUMMY_EMPTY:  
        list_item.DrawNoValue<false>(value, attr);
        break;
      case CHANNEL_SETTING_SCREENSAVER:
        CLOCKS_screensaver(); 
        break;
      case CHANNEL_SETTING_INTERNAL_CLK:
        for (int i = 0; i < 6; i++) 
          clock_channel[i].update_internal_timer(value);
        if (int_clock_used_)  
          list_item.DrawDefault(value, attr);
        break; 
      default:
        list_item.DrawDefault(value, attr);
        break;
    }
  }

  if (clocks_state.pattern_editor.active())
    clocks_state.pattern_editor.Draw();   
}

void Clock_channel::RenderScreensaver(weegfx::coord_t start_x, CLOCK_CHANNEL clock_channel) const {

  uint16_t _square, _frame, _mode = get_mode4();

  _square = TU::OUTPUTS::state(clock_channel);
  _frame  = TU::OUTPUTS::value(clock_channel);

  // DAC needs special treatment:
  if (clock_channel == CLOCK_CHANNEL_4) {

    if (_mode < DAC) {
      _square = _square == ON ? 0x1 : 0x0;
      _frame  = _frame  == ON ? 0x1 : 0x0;
    }
    else { // display DAC values, ish; x/y coordinates slightly off ...
      
      uint16_t _dac_value = _frame;
      
      if (_dac_value < 2047) {
        // output negative
        _dac_value = 16 - (_dac_value >> 7);
        CONSTRAIN(_dac_value, 1, 16);
        graphics.drawFrame(start_x + 5 - (_dac_value >> 1), 41 - (_dac_value >> 1), _dac_value, _dac_value);
      }
      else {
        // positive output
        _dac_value = ((_dac_value - 2047) >> 7);
        CONSTRAIN(_dac_value, 1, 16);
        graphics.drawRect(start_x + 5 - (_dac_value >> 1), 41 - (_dac_value >> 1), _dac_value, _dac_value);
      }
      return;
    }
  }
  
  // draw little square thingies ..     
  if (_square && _frame) {
    graphics.drawRect(start_x, 36, 10, 10);
    graphics.drawFrame(start_x-2, 34, 14, 14);
  }
  else if (_square)
    graphics.drawRect(start_x, 36, 10, 10);
  else
   graphics.drawRect(start_x+3, 39, 4, 4);
}

void CLOCKS_screensaver() {

  clock_channel[0].RenderScreensaver(4,  CLOCK_CHANNEL_1);
  clock_channel[1].RenderScreensaver(26, CLOCK_CHANNEL_2);
  clock_channel[2].RenderScreensaver(48, CLOCK_CHANNEL_3);
  clock_channel[3].RenderScreensaver(70, CLOCK_CHANNEL_4);
  clock_channel[4].RenderScreensaver(92, CLOCK_CHANNEL_5);
  clock_channel[5].RenderScreensaver(114,CLOCK_CHANNEL_6);
}
