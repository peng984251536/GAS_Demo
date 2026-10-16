// 纯伤害公式的独立测试，不需要 Unreal：Tests/Standalone/RunDamageCalculatorTests.cmd
#include "Damage/CC_DamageFormula.h"

#include <iostream>
#include <limits>

int main()
{
    using namespace CCDamageFormula;
    int Failures = 0;
    auto Check = [&](bool Condition, const char* Description)
    {
        if (!Condition)
        {
            ++Failures;
            std::cerr << "FAIL: " << Description << '\n';
        }
    };
    auto Near = [](double A, double B) { return std::abs(A - B) < 0.000001; };
    double Damage = -1.0;

    FInput Input;
    Input.AttackPower = 100.0;
    Input.SkillScale = 2.0;
    Input.FlatDamage = 20.0;
    Input.OutgoingBonus = 0.5;
    Input.CriticalMultiplier = 2.0;
    Input.Defense = 30.0;
    Input.IncomingMultiplier = 0.8;
    Check(Calculate(Input, Damage) && Near(Damage, 504.0), "(100*2+20)*1.5*2-30 then *0.8 = 504");

    Input = FInput{};
    Input.AttackPower = 100.0;
    Input.DamageMultiplier = 0.5;
    Check(Calculate(Input, Damage) && Near(Damage, 50.0), "skill-side multiplier applies before defense");
    Input.Defense = 1000.0;
    Check(Calculate(Input, Damage) && Damage == 0.0, "high defense does not heal");

    Input = FInput{};
    Input.SkillScale = 0.0;
    Input.FlatDamage = 25.0;
    Input.AttackPower = 999.0;
    Check(Calculate(Input, Damage) && Near(Damage, 25.0), "flat-only skill ignores attack");
    Input.OutgoingBonus = -1.0;
    Check(Calculate(Input, Damage) && Damage == 0.0, "-100% bonus is a valid zero");
    Input.OutgoingBonus = -1.5;
    Check(!Calculate(Input, Damage) && Damage == 0.0, "bonus below -1 rejected");

    Input = FInput{};
    Input.Defense = -1.0;
    Check(!Calculate(Input, Damage), "negative defense rejected");
    Input.Defense = std::numeric_limits<double>::quiet_NaN();
    Check(!Calculate(Input, Damage), "NaN rejected");
    Input.Defense = std::numeric_limits<double>::infinity();
    Check(!Calculate(Input, Damage), "infinity rejected");

    Input = FInput{};
    Input.AttackPower = std::numeric_limits<double>::max();
    Input.SkillScale = 2.0;
    Input.DamageMultiplier = 0.0;
    Check(!Calculate(Input, Damage), "later zero multiplier cannot hide overflow");

    if (Failures == 0) std::cout << "All damage formula tests passed\n";
    return Failures == 0 ? 0 : 1;
}
