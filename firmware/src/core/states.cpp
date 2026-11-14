#include <deskquad/core/states.h>

ActionDecision DecideAction(int raw)
{
    ActionDecision decision{ActionDecision::Kind::None, ActionId::None, ACTION_NONE, EmojiId::Happy};
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
    else if (raw >= static_cast<int>(ACTION_SHOW_CLOCK) && raw <= static_cast<int>(ACTION_SHOW_LOGO))
    {
        decision.kind = ActionDecision::Kind::Page;
        switch (static_cast<PageAction>(raw))
        {
        case ACTION_SHOW_CLOCK:
            decision.page = EmojiId::Clock;
            break;
        case ACTION_SHOW_WEATHER:
            decision.page = EmojiId::Weather;
            break;
        case ACTION_SHOW_LOGO:
            decision.page = EmojiId::Logo;
            break;
        }
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
