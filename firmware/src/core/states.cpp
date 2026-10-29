#include <deskquad/core/states.h>

ActionDecision DecideAction(int raw)
{
    ActionDecision decision{ActionDecision::Kind::None, ActionId::None, ACTION_NONE};
    if (raw >= static_cast<int>(ACTION_DANCE) && raw <= static_cast<int>(ACTION_HIDE_FACE))
    {
        decision.kind    = ActionDecision::Kind::Emotion;
        decision.emotion = static_cast<EmotionAction>(raw);
    }
    else if (raw >= static_cast<int>(ActionId::Front) && raw <= static_cast<int>(ActionId::Sleep))
    {
        decision.kind = ActionDecision::Kind::Pose;
        decision.pose = static_cast<ActionId>(raw);
    }
    return decision;
}

bool IsRunnablePose(ActionId id)
{
    const uint8_t value = static_cast<uint8_t>(id);
    // RandomEmote 落在编号区间内，但它由显示层处理，没有对应的姿态序列
    return value >= static_cast<uint8_t>(ActionId::Front) && value <= static_cast<uint8_t>(ActionId::Sleep) &&
           id != ActionId::RandomEmote;
}
