#include <Arduino.h>
#include <NeoPixelBus.h>

namespace
{
constexpr uint8_t DATA_PIN = 5;
constexpr uint8_t PIN_GREEN = 18;
constexpr uint8_t PIN_RED = 19;

constexpr uint16_t NUM_LEDS = 45;
constexpr uint8_t START_ZONE = 3;
constexpr uint8_t BRIGHTNESS = 128;

constexpr uint32_t START_STEP_MS = 120;
constexpr uint32_t MIN_STEP_MS = 25;
constexpr uint32_t SPEEDUP_MS = 6;
constexpr uint32_t INACTIVITY_MS = 1000;
constexpr uint32_t PAUSE_RESET_MS = 10000;
constexpr uint32_t POINT_FLASH_MS = 80;
constexpr uint32_t WIN_FLASH_MS = 180;
constexpr long RANDOM_TWO_OUTCOMES = 2;

const RgbColor COLOR_OFF(0, 0, 0);
const RgbColor COLOR_RED(255, 0, 0);
const RgbColor COLOR_GREEN(0, 255, 0);
const RgbColor COLOR_BALL(255, 255, 255);

enum class SwitchState : uint8_t
{
    Off,
    Green,
    Red,
};

enum class Player : uint8_t
{
    None,
    Red,
    Green,
};

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812Method> strip(NUM_LEDS, DATA_PIN);

uint8_t redZone = START_ZONE;
uint8_t greenZone = START_ZONE;
uint16_t redScore = 0;
uint16_t greenScore = 0;
int16_t ballPos = NUM_LEDS / 2;
int8_t ballDir = 1;
uint32_t stepDelayMs = START_STEP_MS;
uint32_t lastMoveMs = 0;
uint32_t lastInactivityMs = 0;
uint32_t pauseStartedMs = 0;
SwitchState lastSwitchState = SwitchState::Off;
bool paused = true;
bool atBackWall = false;
bool suppressNextResumeAction = true;

RgbColor applyBrightness(const RgbColor& color)
{
    return RgbColor(
        static_cast<uint8_t>((static_cast<uint16_t>(color.R) * BRIGHTNESS) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.G) * BRIGHTNESS) / 255U),
        static_cast<uint8_t>((static_cast<uint16_t>(color.B) * BRIGHTNESS) / 255U));
}

Player otherPlayer(Player player)
{
    if (player == Player::Red)
    {
        return Player::Green;
    }

    if (player == Player::Green)
    {
        return Player::Red;
    }

    return Player::None;
}

SwitchState readSwitchState()
{
    const bool greenActive = digitalRead(PIN_GREEN) == LOW;
    const bool redActive = digitalRead(PIN_RED) == LOW;

    if (greenActive == redActive)
    {
        return SwitchState::Off;
    }

    return greenActive ? SwitchState::Green : SwitchState::Red;
}

Player stateToPlayer(SwitchState state)
{
    switch (state)
    {
        case SwitchState::Red:
            return Player::Red;
        case SwitchState::Green:
            return Player::Green;
        case SwitchState::Off:
        default:
            return Player::None;
    }
}

Player currentDefender()
{
    return ballDir < 0 ? Player::Red : Player::Green;
}

Player backWallDefender()
{
    return ballPos <= 0 ? Player::Red : Player::Green;
}

bool ballIsInZone(Player player)
{
    if (player == Player::Red)
    {
        return ballPos >= 0 && ballPos < redZone;
    }

    if (player == Player::Green)
    {
        return ballPos >= static_cast<int16_t>(NUM_LEDS - greenZone) && ballPos < NUM_LEDS;
    }

    return false;
}

void render()
{
    strip.ClearTo(applyBrightness(COLOR_OFF));

    for (uint8_t i = 0; i < redZone; ++i)
    {
        strip.SetPixelColor(i, applyBrightness(COLOR_RED));
    }

    for (uint8_t i = 0; i < greenZone; ++i)
    {
        strip.SetPixelColor(NUM_LEDS - 1 - i, applyBrightness(COLOR_GREEN));
    }

    if (ballPos >= 0 && ballPos < NUM_LEDS)
    {
        strip.SetPixelColor(ballPos, applyBrightness(COLOR_BALL));
    }

    strip.Show();
}

void freezeGameplayFor(uint32_t durationMs)
{
    const uint32_t start = millis();
    delay(durationMs);
    const uint32_t elapsed = millis() - start;

    lastMoveMs += elapsed;

    if (atBackWall)
    {
        lastInactivityMs += elapsed;
    }
}

void flashStrip(const RgbColor& color, uint32_t durationMs)
{
    strip.ClearTo(applyBrightness(color));
    strip.Show();
    freezeGameplayFor(durationMs);
    render();
}

void reverseBall(uint32_t now)
{
    ballDir = -ballDir;
    const uint32_t nextDelay = stepDelayMs > SPEEDUP_MS ? stepDelayMs - SPEEDUP_MS : 0;
    stepDelayMs = max(MIN_STEP_MS, nextDelay);
    lastMoveMs = now;
    lastInactivityMs = now;
}

void resetTotal()
{
    redZone = START_ZONE;
    greenZone = START_ZONE;
    redScore = 0;
    greenScore = 0;
    ballPos = NUM_LEDS / 2;
    ballDir = random(0, RANDOM_TWO_OUTCOMES) == 0 ? -1 : 1;
    stepDelayMs = START_STEP_MS;
    lastMoveMs = millis();
    lastInactivityMs = lastMoveMs;
    pauseStartedMs = 0;
    atBackWall = false;

    lastSwitchState = readSwitchState();
    paused = lastSwitchState == SwitchState::Off;
    suppressNextResumeAction = paused;

    render();
}

void playVictoryAnimation(Player winner)
{
    const RgbColor winnerColor = winner == Player::Red ? COLOR_RED : COLOR_GREEN;

    for (uint8_t flash = 0; flash < 3; ++flash)
    {
        strip.ClearTo(applyBrightness(winnerColor));
        strip.Show();
        delay(WIN_FLASH_MS);
        strip.ClearTo(applyBrightness(COLOR_OFF));
        strip.Show();
        delay(WIN_FLASH_MS);
    }
}

bool handleVictoryIfNeeded()
{
    Player winner = Player::None;

    if (redZone == 0)
    {
        winner = Player::Green;
    }
    else if (greenZone == 0)
    {
        winner = Player::Red;
    }
    else if (static_cast<uint16_t>(redZone + greenZone) >= NUM_LEDS)
    {
        if (redZone > greenZone)
        {
            winner = Player::Red;
        }
        else if (greenZone > redZone)
        {
            winner = Player::Green;
        }
        else
        {
            winner = random(0, RANDOM_TWO_OUTCOMES) == 0 ? Player::Red : Player::Green;
        }
    }

    if (winner == Player::None)
    {
        return false;
    }

    playVictoryAnimation(winner);
    resetTotal();
    return true;
}

void addPointTo(Player player)
{
    if (player == Player::Red)
    {
        ++redScore;
        ++redZone;
    }
    else if (player == Player::Green)
    {
        ++greenScore;
        ++greenZone;
    }
}

void enterBackWallMode(uint32_t now)
{
    const Player defender = backWallDefender();
    const Player attacker = otherPlayer(defender);

    addPointTo(attacker);

    if (defender == Player::Red)
    {
        redZone = redZone > 0 ? redZone - 1 : 0;
    }
    else
    {
        greenZone = greenZone > 0 ? greenZone - 1 : 0;
    }

    atBackWall = true;
    lastMoveMs = now;
    lastInactivityMs = now;

    if (!handleVictoryIfNeeded())
    {
        render();
        flashStrip(COLOR_BALL, POINT_FLASH_MS);
    }
}

void handleAction(Player actor, uint32_t now)
{
    if (actor == Player::None)
    {
        return;
    }

    if (atBackWall)
    {
        const Player defender = backWallDefender();

        if (actor != defender)
        {
            return;
        }

        reverseBall(now);
        ballPos = defender == Player::Red ? 1 : NUM_LEDS - 2;
        atBackWall = false;
        render();
        return;
    }

    const Player defender = currentDefender();

    if (actor == defender && ballIsInZone(defender))
    {
        reverseBall(now);
        render();
        return;
    }

    addPointTo(otherPlayer(actor));

    if (handleVictoryIfNeeded())
    {
        return;
    }

    reverseBall(now);
    render();
    flashStrip(COLOR_BALL, POINT_FLASH_MS);
}

void handleSwitchChange(SwitchState newState, uint32_t now)
{
    if (newState == lastSwitchState)
    {
        return;
    }

    lastSwitchState = newState;

    if (newState == SwitchState::Off)
    {
        if (!paused)
        {
            paused = true;
            pauseStartedMs = now;
        }

        return;
    }

    if (paused)
    {
        if (pauseStartedMs != 0)
        {
            const uint32_t pauseDuration = now - pauseStartedMs;
            lastMoveMs += pauseDuration;
            lastInactivityMs += pauseDuration;
        }

        paused = false;
        pauseStartedMs = 0;
    }

    if (suppressNextResumeAction)
    {
        suppressNextResumeAction = false;
        return;
    }

    handleAction(stateToPlayer(newState), now);
}
}  // namespace

void setup()
{
    pinMode(PIN_GREEN, INPUT_PULLUP);
    pinMode(PIN_RED, INPUT_PULLUP);

    randomSeed(micros());

    strip.Begin();
    strip.Show();

    resetTotal();
}

void loop()
{
    handleSwitchChange(readSwitchState(), millis());

    if (paused)
    {
        const uint32_t now = millis();

        if (pauseStartedMs != 0 && now - pauseStartedMs >= PAUSE_RESET_MS)
        {
            resetTotal();
        }

        return;
    }

    if (atBackWall)
    {
        while (!paused && atBackWall)
        {
            const uint32_t now = millis();
            handleSwitchChange(readSwitchState(), now);

            if (paused || !atBackWall || now - lastInactivityMs < INACTIVITY_MS)
            {
                break;
            }

            lastInactivityMs += INACTIVITY_MS;

            if (backWallDefender() == Player::Red)
            {
                redZone = redZone > 0 ? redZone - 1 : 0;
            }
            else
            {
                greenZone = greenZone > 0 ? greenZone - 1 : 0;
            }

            if (handleVictoryIfNeeded())
            {
                return;
            }

            render();
        }

        return;
    }

    bool moved = false;

    while (!paused && !atBackWall)
    {
        const uint32_t now = millis();
        handleSwitchChange(readSwitchState(), now);

        if (paused || atBackWall || now - lastMoveMs < stepDelayMs)
        {
            break;
        }

        lastMoveMs += stepDelayMs;
        ballPos += ballDir;
        moved = true;

        if (ballPos <= 0)
        {
            ballPos = 0;
            enterBackWallMode(lastMoveMs);
            return;
        }

        if (ballPos >= NUM_LEDS - 1)
        {
            ballPos = NUM_LEDS - 1;
            enterBackWallMode(lastMoveMs);
            return;
        }
    }

    if (moved)
    {
        render();
    }
}
