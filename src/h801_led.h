
// Led gamma table
// https://learn.adafruit.com/led-tricks-gamma-correction/the-longer-fix
static const uint16_t s_GammaTable[] = {
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,    3,    3,
     3,    3,    4,    4,    4,    5,    5,    5,    6,    6,    7,    7,    7,    8,    8,    9,
    10,   10,   11,   11,   12,   13,   13,   14,   15,   15,   16,   17,   18,   19,   20,   20,
    21,   22,   23,   24,   25,   26,   27,   29,   30,   31,   32,   33,   35,   36,   37,   38,
    40,   41,   43,   44,   46,   47,   49,   50,   52,   54,   55,   57,   59,   61,   63,   64,
    66,   68,   70,   72,   74,   77,   79,   81,   83,   85,   88,   90,   92,   95,   97,  100,
   102,  105,  107,  110,  113,  115,  118,  121,  124,  127,  130,  133,  136,  139,  142,  145,
   149,  152,  155,  158,  162,  165,  169,  172,  176,  180,  183,  187,  191,  195,  199,  203,
   207,  211,  215,  219,  223,  227,  232,  236,  240,  245,  249,  254,  258,  263,  268,  273,
   277,  282,  287,  292,  297,  302,  308,  313,  318,  323,  329,  334,  340,  345,  351,  357,
   362,  368,  374,  380,  386,  392,  398,  404,  410,  417,  423,  429,  436,  442,  449,  455,
   462,  469,  476,  483,  490,  497,  504,  511,  518,  525,  533,  540,  548,  555,  563,  571,
   578,  586,  594,  602,  610,  618,  626,  634,  643,  651,  660,  668,  677,  685,  694,  703,
   712,  721,  730,  739,  748,  757,  766,  776,  785,  795,  804,  814,  824,  833,  843,  853,
   863,  873,  884,  894,  904,  915,  925,  936,  946,  957,  968,  979,  990, 1001, 1012, 1023
};

// Number of steps to to fade each second
#define H801_DURATION_FADE_STEPS 10


class H801_Led {
private:
  String m_id;
  uint8_t m_pin;
  uint8_t m_bri;

  float    m_fadeBri;
  float    m_fadeStep;
  uint32_t m_fadeNum;

  /**
   * Write to data pin
   */
  void updatePin() {
    analogWrite(m_pin, s_GammaTable[m_bri]);
  }

public:
  H801_Led(String id, uint8_t pin) {
    m_id = id;
    m_pin = pin;
    m_bri = 0;

    m_fadeBri = 0;
    m_fadeStep = 0;
    m_fadeNum = 0;

    pinMode(m_pin, OUTPUT);
    updatePin();
  }

  /**
   * Get ID for pin
   * @return ID
   */
  String get_ID() {
    return m_id;
  }

  /**
   * Return current brightness
   * @return brightness
   */
  uint8_t get_Bri() {
    return m_bri; 
  }

  /**
   * Set brightness using JsonVariant
   * @param  item      JSON object value
   * @param  fadeSteps Number of steps to fade over
   * @return Was the led value changed
   */
  bool set_Bri(JsonVariant &item, uint32_t fadeSteps) {
    extern bool stringToInt(const char *psz, int *dest);

    // Number
    if (item.is<int>()) {
      return this->set_Bri((uint8_t)constrain(item.as<int>(), 0x00, 0xFF), fadeSteps);
    }

    // string
    if (item.is<char*>()) {
      int tmp = 0;
      // Parse string
      if (!stringToInt(item.as<char*>(), &tmp))
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
    if (m_bri == bri)
      return false;

    // No duration update pwm directly
    if (!fadeSteps) {
      m_bri = bri;
      updatePin();
      return true;
    }


    // Calculate step size and set fading flag
    // Set the bri to the target value so the change event contains the target values
    m_fadeStep = (float)((int)bri - (int)m_bri)/(float)fadeSteps;
    m_fadeBri = m_bri;
    m_fadeNum = fadeSteps;

    // Set current bri to target so get_Bri returns correct value, 
    // this value will be overwritten on first do_Fade before updating pin
    m_bri = bri;

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

    // Multiply 
    m_fadeBri += m_fadeStep;
    m_fadeBri = constrain(m_fadeBri, 0x00, 0xFF);

    // Update brghtness 
    m_bri = (uint8_t)(m_fadeBri + 0.5);
    updatePin();

    // Last fade event, clear values
    if (!m_fadeNum) {
      m_fadeBri = 0;
      m_fadeStep = 0;
      return false;
    }

    // Still fading
    return true;
  }
};

