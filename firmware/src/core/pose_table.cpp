#include <deskquad/core/pose_table.h>

namespace
{

    constexpr int16_t H = kHoldAngle;

    const PoseStep kFrontSteps[] = {
        {{H, 140, 40, H}, 100}, {{40, H, H, 140}, 100}, {{H, 90, 90, H}, 100}, {{90, H, H, 90}, 100},
        {{140, H, H, 40}, 100}, {{H, 40, 140, H}, 100}, {{90, H, H, 90}, 100}, {{H, 90, 90, H}, 0},
    };

    const PoseStep kBackSteps[] = {
        {{H, 40, 140, H}, 100}, {{140, H, H, 40}, 100}, {{H, 90, 90, H}, 100}, {{90, H, H, 90}, 100},
        {{40, H, H, 140}, 100}, {{H, 140, 40, H}, 100}, {{90, H, H, 90}, 100}, {{H, 90, 90, H}, 0},
    };

    const PoseStep kRightSteps[] = {
        {{100, H, H, 100}, 100}, {{H, 60, 60, H}, 100}, {{140, H, H, 140}, 100}, {{H, 40, 40, H}, 100},
        {{90, 90, 90, 90}, 100}, {{80, H, H, 80}, 100}, {{H, 120, 120, H}, 100}, {{90, H, H, 90}, 100},
        {{H, 140, 140, H}, 100}, {{H, 90, 90, H}, 0},
    };

    const PoseStep kLeftSteps[] = {
        {{80, H, H, 80}, 100},   {{H, 120, 120, H}, 100}, {{40, H, H, 40}, 100}, {{H, 140, 140, H}, 100},
        {{90, 90, 90, 90}, 100}, {{100, H, H, 100}, 100}, {{H, 60, 60, H}, 100}, {{90, H, H, 90}, 100},
        {{H, 40, 40, H}, 100},   {{H, 90, 90, H}, 0},
    };

    const PoseStep kSitDownSteps[] = {
        {{H, 140, H, 40}, 3000},
        {{H, 90, H, 90}, 0},
    };

    const PoseStep kLieSteps[] = {
        {{180, 0, 0, 180}, 3000},
        {{90, 90, 90, 90}, 0},
    };

    const PoseStep kHandLeftSteps[] = {
        {{H, H, 0, H}, 100},
        {{H, H, 30, H}, 100},
        {{H, H, 90, H}, 0},
    };

    const PoseStep kHandRightSteps[] = {
        {{180, H, H, H}, 100},
        {{150, H, H, H}, 100},
        {{90, H, H, H}, 0},
    };

    const PoseStep kSleepSteps[] = {
        {{0, 180, 180, 0}, 0},
    };

    constexpr PoseProgram kPrograms[] = {
        {nullptr, 0, 0, 1},         // None
        {kFrontSteps, 8, 8, 1},     // Front
        {kLeftSteps, 10, 10, 3},    // Left
        {kRightSteps, 10, 10, 3},   // Right
        {kBackSteps, 8, 8, 1},      // Back
        {kHandLeftSteps, 3, 2, 3},  // HandLeft
        {kHandRightSteps, 3, 2, 3}, // HandRight
        {kLieSteps, 2, 2, 1},       // Lie
        {kSitDownSteps, 2, 2, 1},   // Sit
        {nullptr, 0, 0, 1},         // RandomEmote
        {kSleepSteps, 1, 1, 1},     // Sleep
    };

    constexpr uint8_t kProgramCount = sizeof(kPrograms) / sizeof(kPrograms[0]);

} // namespace

const PoseProgram *FindPoseProgram(ActionId id)
{
    const uint8_t index = static_cast<uint8_t>(id);
    if (index >= kProgramCount)
        return nullptr;
    const PoseProgram &program = kPrograms[index];
    return program.steps != nullptr ? &program : nullptr;
}
