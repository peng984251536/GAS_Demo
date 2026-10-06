#include "Damage/StandaloneDamageCalculator.h"

#include <iostream>
#include <limits>

int main()
{
    using namespace StandaloneDamage;
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

    FInput Input;
    Input.AttackPower = 100.0;
    Input.SkillScale = 2.0;
    Input.FlatDamage = 20.0;
    Input.DistanceMultiplier = 0.5;
    Input.HitMultiplier = 2.0;
    Input.Defense = 30.0;
    Input.IncomingMultiplier = 0.8;
    const FResult Result = Calculate(Input);
    Check(Result.IsValid() && Near(Result.FinalDamage, 152.0), "skill, distance, hit, defense, incoming use documented order");
    Check(Near(Result.RawDamage, 220.0) && Near(Result.DamageAfterDefense, 190.0), "intermediate values describe the hit");
    Check(Near(Input.AttackPower, 100.0), "input is unchanged");

    Input.bDamageAllowed = false;
    Check(Calculate(Input).IsValid() && Calculate(Input).FinalDamage == 0.0, "team/immunity veto produces no damage");
    Input.bDamageAllowed = true;
    Input.Defense = 1000.0;
    Check(Calculate(Input).FinalDamage == 0.0, "high defense does not heal");
    Input.Defense = 0.0;
    Input.HitMultiplier = 0.0;
    Check(Calculate(Input).FinalDamage == 0.0, "zero hit multiplier is a valid zero-damage hit");

    Input = FInput{};
    Input.AttackPower = 150.0;
    Check(Calculate(Input).FinalDamage == 150.0, "already buffed attack is consumed once");
    Input.SkillScale = 0.0;
    Input.FlatDamage = 25.0;
    Check(Calculate(Input).FinalDamage == 25.0, "flat-only skill ignores attack");

    Input.Defense = -1.0;
    Check(Calculate(Input).Error == EError::InvalidInput, "negative defense rejected");
    Input.Defense = std::numeric_limits<double>::quiet_NaN();
    Check(Calculate(Input).Error == EError::InvalidInput, "NaN rejected");
    Input.Defense = std::numeric_limits<double>::infinity();
    Check(Calculate(Input).Error == EError::InvalidInput, "infinity rejected");

    Input = FInput{};
    Input.AttackPower = std::numeric_limits<double>::max();
    Input.SkillScale = 2.0;
    Input.DistanceMultiplier = 0.0;
    Check(Calculate(Input).Error == EError::Overflow, "later zero multiplier cannot hide overflow");
    Input = FInput{};
    Input.AttackPower = 1.0;
    Input.IncomingMultiplier = std::numeric_limits<double>::max();
    Input.HitMultiplier = 2.0;
    Check(Calculate(Input).Error == EError::Overflow, "final-stage overflow rejected");

    std::cout << (Failures == 0 ? "PASS" : "FAIL") << ": standalone damage scenarios; failures=" << Failures << '\n';
    return Failures == 0 ? 0 : 1;
}
