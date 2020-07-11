// proof of concept: WS2818 strip on ESP32
// Using ESP32 Arduino core and AdaFruit NeoPixel library (latter to be replaced)

#include <dummy.h>
#include <Adafruit_NeoPixel.h>

// gamma8 table from Adafruit
const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

// Original code as of here
// Put in the public domain

// 2 strips with 38 3-LED groups each on GPIO4
#define PIN 4
#define NLEDS (38*2)
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NLEDS, PIN, NEO_BRG + NEO_KHZ800);

#define WARM_WHITE    255, 224, 128
#define NEUTRAL_WHITE 255, 208, 160
#define COLD_WHITE    128, 224, 255

// GPIO IDs for button panel
const auto BUTTON_PINS = {25, 33, 32, 35, 34};


class Button {
  const static auto num_bits = 32;
  const static auto hysteresis = 0;
public:
  Button(int pin)
  : pin(pin) {}

  void begin() {
    pinMode(pin, INPUT);
    reset();
  }

  void reset() {
    level = 0;
    history = 0;
    active_ = 0;
  }

  bool check() {
    bool current = digitalRead(pin);
    if (history & 1) {
      --level;
    }
    history >>= 1;
    if (current) {
      history |= static_cast<uint32_t>(1)<<(num_bits-1);
      ++level;
    }
    if (pin == 33) {
      //Serial.println(history);
    }
    if (level <= hysteresis) {
      return active_ = false;
    }
    else if (level >= num_bits-hysteresis) {
      return active_ = true;
    }
    else {
      return active_;
    }
  }

  bool active() {
    return active_;
  }

  bool pressed() {
    const auto was_pressed = active_;
    const bool newly_pressed = check() && !was_pressed;
    if (newly_pressed) {
      Serial.println("Button press #" + String(pin));
    }
    return newly_pressed;
  }

private:
  uint32_t history;
  uint16_t pin;
  uint8_t level;
  bool active_;
};

Button btnPower(25);
Button btnPrev(33);
Button btnNext(32);
Button btnDark(35);
Button btnBright(34);
Button btnDarkHeld(35);
Button btnBrightHeld(34);

void setup()
{
  pixels.begin(); // This initializes the NeoPixel library.
  Serial.begin(115200);
  portDISABLE_INTERRUPTS();
  pixels.show();
  delay(2);
  pixels.show();
  portENABLE_INTERRUPTS();
  Serial.println("Start!");

  btnPower.begin();
  btnPrev.begin();
  btnNext.begin();
  btnDark.begin();
  btnBright.begin();
  btnDarkHeld.begin();
  btnBrightHeld.begin();
}

const auto display_ratio = 16;
const auto display_space = 0;
const uint8_t colors[][3] = {
  {WARM_WHITE},
  {255, 0, 0}, {255, 128, 0}, {255, 224, 0}, {0, 192, 0},
  {0, 160, 192}, {0, 0, 224}, {128, 0, 160}, {255, 0, 128}
};
const auto num_colors = sizeof(colors)/sizeof(*colors);
bool active = false;
int current_color_index = 0;
uint8_t current_color[3] = {NEUTRAL_WHITE};
const uint8_t black[3] = {0, 0, 0};
const int initial_color_step = -2*display_ratio-display_space;
int color_step = initial_color_step;
int brightness_modifier = 0;
bool color_update_flag = false;
bool brightness_reset = false;

bool done = true;
bool animate() {
  bool needs_refresh = false;
  auto set_color
    = [](unsigned led, const uint8_t color[], unsigned ratio_n = 1, unsigned ratio_d = 1) {
      if (led < 0 || led >= NLEDS/2) return;
      auto color_mod
        = [&](int input) {
          return gamma8[
            (3 * (input*ratio_n+ratio_d/2))
            /
            (4 * ratio_d)
          ];
        };
      const auto r = color_mod(color[0]);
      const auto g = color_mod(color[1]);
      const auto b = color_mod(color[2]);
      pixels.setPixelColor(led, r, g, b);
      pixels.setPixelColor(NLEDS-1-led, r, g, b);
    };

  if (color_update_flag) {
    needs_refresh = true;
    for(int led = 0; led < color_step; ++led) {
      set_color(led, current_color);
    }
    color_update_flag = false;
  }

  if (!done) {
    needs_refresh = true;
    int led=color_step;

    // new color fade in
    for(int i=display_ratio; i>0; ++led, --i) {
      set_color(led, current_color, i, display_ratio);
    }

    // black gap
    for(int i=0; i<display_space; ++led, ++i) {
      set_color(led, black);
    }

    // old color fade out
    for(int i=1; i<display_ratio; ++led, ++i) {
      const auto stored_color = pixels.getPixelColor(led);
      const uint8_t stored_color_array[]
        = {
          (uint8_t)(stored_color >> 16),
          (uint8_t)(stored_color >> 8),
          (uint8_t)stored_color
        };
      set_color(led, stored_color_array, i, i+1);
    }
    
    ++color_step;
    if (color_step >= NLEDS/2) {
      done = true;
    }
  }

  if (needs_refresh) {
    portDISABLE_INTERRUPTS();
    pixels.show();
    portENABLE_INTERRUPTS();
  }
  else {
    delay(1);
  }
  return done;
}

void update_color() {
  auto mod
    = [](int level) {
      if (brightness_modifier <= 0) {
        return level*(brightness_modifier+32)/32;
      }
      else {
        return (255*brightness_modifier+level*(32-brightness_modifier))/32;
      }
    };
  current_color[0] = mod(colors[current_color_index][0]);
  current_color[1] = mod(colors[current_color_index][1]);
  current_color[2] = mod(colors[current_color_index][2]);
  Serial.println("Target color: " + String(current_color[0]) + "/" + String(current_color[1]) + "/" + String(current_color[2]));
}

void reset_brightness() {
  brightness_reset = true;
  brightness_modifier = 0;
  update_color();
}

void start_animation() {
  color_step = initial_color_step;
  done = false;
}

void onoff() {
  active = !active;
  
  if (active) {
    update_color();
  }
  else {
    current_color[0] = 0;
    current_color[1] = 0;
    current_color[2] = 0;
  }

  start_animation();
}

const auto animate_frames = 15;
int frame = animate_frames-1;
void loop()
{
  if (btnPower.pressed()) {
    onoff();
  }

  if (btnPrev.pressed()) {
    current_color_index = (current_color_index + num_colors - 1) % num_colors;
    if (active) {
      update_color();
      start_animation();
    }
  }
  if (btnNext.pressed()) {
    current_color_index = (current_color_index + 1) % num_colors;
    if (active) {
      update_color();
      start_animation();
    }
  }

  btnDarkHeld.check();
  btnBrightHeld.check();
  if (!btnDarkHeld.active() && !btnDarkHeld.active()) {
    brightness_reset = false;
  }
  
  if (btnBright.pressed()) {
    if (btnDarkHeld.active()) {
      reset_brightness();
    }
    if (!brightness_reset) {
      btnBright.reset();
      if (brightness_modifier < 30) {
        ++brightness_modifier;
        update_color();
        color_update_flag=true;
      }
    }
  }
  if (btnDark.pressed()) {
    if (btnBrightHeld.active()) {
      reset_brightness();
    }
    if (!brightness_reset) {
      btnDark.reset();
      if (brightness_modifier > -26) { // don't allow all the way to -16, as black cannot be distinguished from off
        --brightness_modifier;
        update_color();
        color_update_flag=true;
      }
    }
  }

  if (++frame == animate_frames) {
    frame = 0;
    animate();
  }
  delay(2);
}
