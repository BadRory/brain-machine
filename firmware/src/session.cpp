// session.cpp — JSON (de)serialization + built-in presets.
#include "session.h"

bool Session::fromJson(const JsonObjectConst& o) {
  if (o.isNull()) return false;
  id   = o["id"]   | "";
  name = o["name"] | "";
  category = o["category"] | "custom";
  blurb    = o["blurb"]    | "";
  if (id.isEmpty()) return false;

  steps.clear();
  for (JsonObjectConst s : o["steps"].as<JsonArrayConst>()) {
    Step st;
    st.hz      = s["hz"]      | 10.0f;
    st.minutes = s["minutes"] | 2.0f;
    st.fade_s  = s["fade_s"]  | 30.0f;
    st.carrier_hz = s["carrier_hz"] | 0.0f;
    steps.push_back(st);
  }
  if (steps.empty()) return false;

  JsonObjectConst l = o["light"];
  if (!l.isNull()) {
    light.brightness = l["brightness"] | light.brightness;
    light.pattern    = l["pattern"]    | light.pattern;
  }
  JsonObjectConst a = o["audio"];
  if (!a.isNull()) {
    audio.carrier_hz = a["carrier_hz"] | audio.carrier_hz;
    audio.tone       = a["tone"]       | audio.tone;
    audio.soundscape = a["soundscape"] | audio.soundscape;
    audio.volume     = a["volume"]     | audio.volume;
    audio.noise_vol  = a["noise_vol"]  | audio.noise_vol;
  }
  return true;
}

void Session::toJson(JsonObject o) const {
  o["id"]   = id;
  o["name"] = name;
  o["category"] = category;
  o["blurb"] = blurb;
  JsonArray arr = o["steps"].to<JsonArray>();
  for (const auto& st : steps) {
    JsonObject s = arr.add<JsonObject>();
    s["hz"]      = st.hz;
    s["minutes"] = st.minutes;
    s["fade_s"]  = st.fade_s;
    if (st.carrier_hz > 0) s["carrier_hz"] = st.carrier_hz;
  }
  JsonObject l = o["light"].to<JsonObject>();
  l["brightness"] = light.brightness;
  l["pattern"]    = light.pattern;
  JsonObject a = o["audio"].to<JsonObject>();
  a["carrier_hz"] = audio.carrier_hz;
  a["tone"]       = audio.tone;
  a["soundscape"] = audio.soundscape;
  a["volume"]     = audio.volume;
  a["noise_vol"]  = audio.noise_vol;
}

static Session makeClassic() {
  // Mitch Altman's original 14-min arc: beta -> alpha -> theta -> back up.
  // Credit: Mitch Altman, Brain Machine / Sound & Light Machine (CC BY-SA).
  Session s;
  s.id = "classic-14";
  s.name = "Classic (Mitch Altman's original)";
  s.category = "classic";
  s.blurb = "The original 14 minute arc: alert, down to theta, and back up.";
  s.steps = {
    { 20.0f, 1.0f, 10.0f },
    { 15.0f, 2.0f, 30.0f },
    { 10.0f, 3.0f, 60.0f },
    {  5.0f, 4.0f, 60.0f },
    { 10.0f, 2.0f, 60.0f },
    { 15.0f, 1.0f, 30.0f },
    { 20.0f, 1.0f, 30.0f },
  };
  return s;
}

static Session makeSimple(const char* id, const char* name, const char* cat,
                          const char* blurb, float hz, float minutes,
                          float carrier, const char* tone,
                          const char* bed) {
  Session s;
  s.id = id; s.name = name; s.category = cat; s.blurb = blurb;
  s.steps = { { hz, minutes, 60.0f } };
  s.audio.carrier_hz = carrier;
  s.audio.tone = tone;
  s.audio.soundscape = bed;
  if (hz < 4.0f) s.light.brightness = 0.25f;   // slow flashes feel harsher: keep dim
  return s;
}

static Session makeWinddown() {
  Session s;
  s.id = "evening-winddown";
  s.name = "Evening wind-down";
  s.category = "sleep";
  s.blurb = "16 minutes from calm alpha down to theta and delta, with soft rain.";
  s.steps = {
    { 12.0f, 2.0f, 30.0f },
    { 10.0f, 4.0f, 60.0f },
    {  6.0f, 6.0f, 60.0f },
    {  4.0f, 4.0f, 60.0f },
  };
  s.audio.carrier_hz = 200.0f;
  s.audio.soundscape = "rain";
  s.light.brightness = 0.4f;
  return s;
}

static Session makeFocus() {
  Session s;
  s.id = "morning-focus";
  s.name = "Morning focus";
  s.category = "focus";
  s.blurb = "10 minutes of calm alertness at low beta, with pink noise.";
  s.steps = {
    { 10.0f, 2.0f, 20.0f },
    { 14.0f, 4.0f, 45.0f },
    { 12.0f, 4.0f, 45.0f },
  };
  s.audio.carrier_hz = 220.0f;
  s.audio.soundscape = "pink";
  return s;
}

// Chakra tones. These are the "Solfeggio" set most chakra videos use
// (396 root ... 963 crown). Each session plays that tone as the carrier, with
// a gentle octave above it, and a slow beat that settles from alpha to theta.
struct Chakra { const char* id; const char* name; float hz; float beat; const char* note; };
static const Chakra kChakras[] = {
  { "chakra-root",     "Root chakra (396 Hz)",         396.0f, 7.0f, "Grounding. Slow theta." },
  { "chakra-sacral",   "Sacral chakra (417 Hz)",       417.0f, 7.0f, "Flow and release. Theta." },
  { "chakra-solar",    "Solar plexus chakra (528 Hz)", 528.0f, 8.0f, "Energy and will. Low alpha." },
  { "chakra-heart",    "Heart chakra (639 Hz)",        639.0f, 8.0f, "Calm and connection. Low alpha." },
  { "chakra-throat",   "Throat chakra (741 Hz)",       741.0f, 9.0f, "Clear and open. Alpha." },
  { "chakra-third",    "Third eye chakra (852 Hz)",    852.0f, 6.0f, "Deep focus inward. Theta." },
  { "chakra-crown",    "Crown chakra (963 Hz)",        963.0f, 5.0f, "Stillness. Deep theta." },
};

static Session makeChakra(const Chakra& c) {
  Session s;
  s.id = c.id; s.name = c.name; s.category = "chakra";
  s.blurb = String(c.note) + " 12 minutes.";
  s.steps = { { 10.0f, 2.0f, 5.0f }, { c.beat, 10.0f, 90.0f } };
  s.audio.carrier_hz = c.hz;
  s.audio.tone = "chakra";
  s.audio.soundscape = "none";
  s.audio.volume = 0.5f;
  return s;
}

static Session makeChakraJourney() {
  Session s;
  s.id = "chakra-journey";
  s.name = "Chakra journey (all seven)";
  s.category = "chakra";
  s.blurb = "Root to crown, 3 minutes each, tone glides between them. 21 minutes.";
  for (const Chakra& c : kChakras) {
    Step st; st.hz = c.beat; st.minutes = 3.0f; st.fade_s = 20.0f; st.carrier_hz = c.hz;
    s.steps.push_back(st);
  }
  s.audio.carrier_hz = 396.0f;
  s.audio.tone = "chakra";
  s.audio.soundscape = "ocean";
  s.audio.noise_vol = 0.2f;
  s.audio.volume = 0.5f;
  return s;
}

const std::vector<Session>& builtinSessions() {
  static std::vector<Session> presets = [] {
    std::vector<Session> v;
    v.push_back(makeClassic());
    v.push_back(makeSimple("alpha-relax", "Alpha relax (10 Hz)", "relax",
        "Steady 10 Hz. Light, relaxed, awake. 15 minutes.",
        10.0f, 15.0f, 200.0f, "binaural", "none"));
    v.push_back(makeSimple("theta-meditate", "Theta meditation (6 Hz)", "relax",
        "Steady 6 Hz for deep meditation. 20 minutes.",
        6.0f, 20.0f, 200.0f, "binaural", "ocean"));
    v.push_back(makeSimple("schumann", "Schumann resonance (7.83 Hz)", "relax",
        "Steady 7.83 Hz, the 'earth hum' many meditation tracks use. 20 minutes.",
        7.83f, 20.0f, 180.0f, "binaural", "pink"));
    v.push_back(makeSimple("delta-sleep", "Delta sleep (2 Hz)", "sleep",
        "Slow 2 Hz for falling asleep. 30 minutes. Light stays dim.",
        2.0f, 30.0f, 150.0f, "binaural", "brown"));
    v.push_back(makeSimple("iso-alpha", "Isochronic alpha (10 Hz)", "relax",
        "Pulsing tone instead of a binaural beat. Works on speakers too. 15 minutes.",
        10.0f, 15.0f, 250.0f, "isochronic", "none"));
    v.push_back(makeWinddown());
    v.push_back(makeFocus());
    for (const Chakra& c : kChakras) v.push_back(makeChakra(c));
    v.push_back(makeChakraJourney());
    return v;
  }();
  return presets;
}
