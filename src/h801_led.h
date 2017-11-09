
// Led gamma table
// https://learn.adafruit.com/led-tricks-gamma-correction/the-longer-fix
static const uint16_t s_gammaTable[] = {
#include "gammatable.inc"
};

// Ensure gammatable is correct length
static_assert(countof(s_gammaTable) == 1024, "Gamma-table countof must be 1024");

/**
 * H801 Led
 */
class H801_Led {
private:
  String   m_id;
  uint8_t  m_pwm_index;
  int      m_bri;

  double    m_fadeBri;
  double    m_fadeStep;
  uint32_t  m_fadeNum;

public:
  H801_Led(String id) {
    m_id = id;
    m_pwm_index = 0;
    m_bri = 0;

    m_fadeBri = 0;
    m_fadeStep = 0;
    m_fadeNum = 0;

  }

  
  void setup(uint8_t pin, uint8_t pwm_index) {
    pinMode(pin, OUTPUT);
    m_pwm_index = pwm_index;
  }


  /**
   * Get ID for pin
   * @return ID
   */
  String& get_ID() {
    return m_id;
  }


  /**
   * Return current brightness
   * @return brightness
   */
  uint8_t get_Bri() {
    return m_bri>>2; 
  }


  /**
   * Return current brightness
   * @return brightness
   */
  uint16_t get_RawBri() {
    return m_bri; 
  }


  /**
   * Set brightness using JsonVariant
   * @param  item      JSON object value
   * @param  fadeSteps Number of steps to fade over
   * @return Was the led value changed
   */
  bool set_Bri(JsonVariant &item, uint32_t fadeSteps) {
    // Number
    if (item.is<int>()) {
      return this->set_Bri((uint8_t)constrain(item.as<int>(), 0x00, 0xFF), fadeSteps);
    }

    // string
    if (item.is<char*>()) {
      unsigned long tmp = 0;
      // Parse string
      if (!stringToUnsignedLong(item.as<char*>(), &tmp))
        return false;

      return this->set_Bri((uint8_t)constrain(tmp, 0x00, 0xFF), fadeSteps);
    }

    // Unhandled value type
    return false;
  }

  /**
   * Set brightness for led
   * @param  bri       New brightness
   * @param  fadeSteps Number of steps to fade over
   * @return Was the led value changed
   */
  bool set_Bri(uint8_t bri, uint32_t fadeSteps) {
    // Clear any current fading
    m_fadeNum = 0;

    // No change
    if (m_bri>>2 == bri)
      return false;

    // No duration update pwm directly
    if (!fadeSteps) {
      m_bri = (bri<<2 | 0x3) & 0x3FF;
      pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
      return true;
    }


    // Calculate step size and set fading flag
    // Set the bri to the target value so the change event contains the target values
    int fadeDiff = (int16_t)(((bri<<2) - m_bri) | 0x3);
    m_fadeStep = (double)(fadeDiff)/(double)(fadeSteps);
    m_fadeBri = m_bri;
    m_fadeNum = fadeSteps + 1;

    // Set current bri to target so get_Bri returns correct value, 
    // this value will be overwritten on first do_Fade before updating pin
    m_bri = (bri<<2 | 0x3) & 0x3FF;

    return true;
  }

  /**
   * Fade led value one step
   * @return Are we still fading
   */
  bool do_Fade() {
    // Fading light?
    if (!m_fadeNum)
      return false;

    // Decrease fading counter
    m_fadeNum--;

    // Fade one step
    m_fadeBri += m_fadeStep;
    m_fadeBri = constrain(m_fadeBri, 0x0, 0x3FF);

    // Convert to 10-bit number
    m_bri = ((int)(m_fadeBri)) & 0x3FF;

    // Update light
    pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
    //analogWrite(m_pin, s_gammaTable[m_bri]);
    
    // Last fade event, clear values
    if (!m_fadeNum) {
      m_fadeBri = 0;
      m_fadeStep = 0;
      return false;
    }

    // Still fading
    return true;
  }

  /**
   * Overrides current fade and fades single step,
   * @param dirUp Are we fading toward max
   */
  void do_ButtonFade(bool dirUp) {
    m_fadeBri = 0;
    m_fadeStep = 0;
    m_fadeNum = 0;

    if (dirUp && m_bri < 0x3FF)
      m_bri += 5;
    else if (!dirUp && m_bri > 0)
      m_bri -= 5;

    m_bri = constrain(m_bri, 0x0, 0x3FF);

    // Update light
    pwm_set_duty(s_gammaTable[m_bri], m_pwm_index);
  }
};

