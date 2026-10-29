#!/usr/bin/env bash
# Buff 系统规则检查。
#   在仓库根目录运行：Tools/HarnessChecks/check_buff_rules.sh
#   作为 Claude Code 的 PostToolUse hook：check_buff_rules.sh --hook（从 stdin 读取工具调用 JSON）
#
# 规则说明见 CLAUDE.md 与 Docs/Buff/BuffSystem.zh-CN.md，结构见 Docs/Buff/BuffSystem-Architecture.zh-CN.md。
# ERROR ：违反硬性规则，退出码 1（hook 模式下退出码 2，把报错反馈给 Claude）。
# LEGACY：已知遗留项或前置设施未完成，见 Roadmap，不影响退出码。
#
# 确有理由的例外：在该行末尾写注释  // harness:allow R<编号> <理由>
# 例：  GetWorld()->SpawnActor<AFoo>(...);  // harness:allow R5 召唤物由 Summon 行为自行管理
#
# 兼容过渡期：旧结构（UCC_BuffDefinition / UCC_GE_Buff）与新结构（UCC_BuffUIData /
# UCC_BuffRulesComponent）同时存在时都会检查；某个前置设施还没实现时，相关规则只报 LEGACY。
set -u

MODE="full"
[ "${1:-}" = "--hook" ] && MODE="hook"

if [ "$MODE" = "hook" ]; then
  INPUT="$(cat)"
  FILE_PATH="$(printf '%s' "$INPUT" | grep -o '"file_path"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 | sed 's/.*:[[:space:]]*"\(.*\)"/\1/')"
  case "$FILE_PATH" in
    *Source/GAS_Demo/*|*Source\\\\GAS_Demo\\\\*|*Tools/HarnessChecks/*) ;;
    *) exit 0 ;;   # 只在改动 C++ 源码或本脚本时检查
  esac
  cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
  # hook 模式只报告本次编辑的文件中的错误；存量问题由完整运行负责
  NORM="$(printf '%s' "$FILE_PATH" | sed 's#\\\\#/#g; s#\\#/#g')"
  case "$NORM" in
    *Source/GAS_Demo/*) HOOK_FILE="Source/GAS_Demo/${NORM##*Source/GAS_Demo/}" ;;
    *) HOOK_FILE="" ;;   # 改的是本脚本：报告全部
  esac
fi

SRC="Source/GAS_Demo"
ERRORS=0
LEGACY=0
OUT=""

err()    { OUT+="ERROR  [$1] $2"$'\n'; ERRORS=$((ERRORS+1)); }
legacy() { OUT+="LEGACY [$1] $2"$'\n'; LEGACY=$((LEGACY+1)); }
# 行内写了 harness:allow R<n> 的命中跳过
allowed() { [[ "$1" == *"harness:allow $2"* ]]; }
# 在源码中按文件名定位（目录调整后仍能找到）
find_src() { find "$SRC" -name "$1" -not -path "*/Intermediate/*" 2>/dev/null | head -1; }

if [ ! -d "$SRC" ]; then
  echo "请在仓库根目录运行（找不到 $SRC）"; exit 2
fi

BEHAVIOR_H="$(find_src CC_BuffBehavior.h)"
TEAM_H="$(find_src CC_TeamStatics.h)"
DEF_H="$(find_src CC_BuffDefinition.h)"
UIDATA_H="$(find_src CC_BuffUIData.h)"
RULES_H="$(find_src CC_BuffRulesComponent.h)"
# 自动回收工具是否已在 Behavior 基类中实现（决定 R5 是 ERROR 还是 LEGACY）
HAS_OWNED_HELPERS=0
[ -n "$BEHAVIOR_H" ] && grep -q "ListenGameplayEvent" "$BEHAVIOR_H" && HAS_OWNED_HELPERS=1

# 所有 UCC_BuffBehavior 子类的头文件（不含基类与测试）
BEHAVIOR_SUB_H="$(grep -rlE --include=*.h "public[[:space:]]+UCC_BuffBehavior([^_A-Za-z0-9]|$)" "$SRC" 2>/dev/null | grep -v "/Tests/" || true)"

# ---------------------------------------------------------------------------
# 规则 2：Buff 配置只保存显示信息与生命周期设置，不新增玩法字段
# 用法：check_fields 文件 "允许字段" "遗留字段"
check_fields() {
  local FILE="$1" ALLOWED="$2" LEGACY_FIELDS="$3"
  [ -f "$FILE" ] || return 0
  while IFS=: read -r LINE NAME; do
    allowed "$(sed -n "${LINE}p" "$FILE")" R2 && continue
    if [[ " $ALLOWED " == *" $NAME "* ]]; then continue; fi
    if [[ " $LEGACY_FIELDS " == *" $NAME "* ]]; then
      legacy R2 "$FILE:$LINE 字段 $NAME 待迁移（见 Roadmap 第 5、6 项）"
    else
      err R2 "$FILE:$LINE 新增了玩法字段 $NAME；请改用标签 / GA / UCC_BuffBehavior"
    fi
  done < <(awk '
    function emit(s,   n, parts) {
      sub(/=.*/, "", s); sub(/;.*/, "", s); sub(/[ \t]+$/, "", s)
      n = split(s, parts, /[ \t*&>]+/)
      if (parts[n] != "") print NR ":" parts[n]
    }
    /UPROPERTY\(/ {
      rest = $0
      # 去掉 UPROPERTY(...)，允许一层嵌套括号，如 meta=(...)
      sub(/.*UPROPERTY\(([^()]|\([^()]*\))*\)/, "", rest)
      if (rest ~ /;/) { emit(rest); want = 0 } else want = 1
      next
    }
    want && /;/ { emit($0); want = 0 }' "$FILE")
}

check_fields "$DEF_H"    "DisplayName Description Icon bRemoveOnDeath Behaviors" \
                         "AttackActionSetOverride SummonClass SummonCount SummonRadius bDestroySummonsOnRemoval"
check_fields "$UIDATA_H" "DisplayName Description Icon bHiddenInUI" ""
check_fields "$RULES_H"  "Behaviors bRemoveOnDeath bPersistAcrossLevels" ""
if [ -n "$RULES_H" ] && [ -n "$DEF_H" ]; then
  legacy R2 "$DEF_H 新结构 UCC_BuffRulesComponent 已存在，旧 UCC_BuffDefinition / UCC_GE_Buff 待资产迁移后删除"
fi

# ---------------------------------------------------------------------------
# 规则 3：伤害 GE 只能在 UCC_DamageStatics 中构造
while IFS= read -r HIT; do
  [ -z "$HIT" ] && continue
  allowed "$HIT" R3 && continue
  case "${HIT%%:*}" in
    */Damage/CC_DamageStatics.cpp|*/Tests/*) ;;
    *) err R3 "$HIT  直接构造了伤害 GE；请改用 UCC_DamageStatics::ApplyDamage 并填写 Damage.Source.*" ;;
  esac
done < <(grep -rnE --include=*.cpp "UCC_GE_[A-Za-z0-9_]*Damage[A-Za-z0-9_]*::StaticClass" "$SRC" 2>/dev/null)

# ---------------------------------------------------------------------------
# 规则 1：系统代码不判断具体 Buff（只认标签 / 属性 / 上下文）
SYSTEM_DIRS="$SRC/Private/AbilitySystem $SRC/Private/Actors $SRC/Private/Components $SRC/Private/Damage $SRC/Private/AI"
R1_PATTERN='HasBuff\(|GetBuffStackCount\(|BuffConfiguration|GetAttackActionSetOverride\(|Cast<UCC_BuffDefinition>|Cast<UCC_GE_Buff>|IsA<UCC_GE_Buff>|FindComponent<UCC_BuffRulesComponent>|UCC_BuffBehavior_[A-Za-z0-9]+'
while IFS= read -r HIT; do
  [ -z "$HIT" ] && continue
  allowed "$HIT" R1 && continue
  case "$HIT" in
    *GetAttackActionSetOverride*) legacy R1 "$HIT  Buff 直接提供动作集（见 Roadmap 第 5 项）" ;;
    *) err R1 "$HIT  系统代码在判断具体 Buff；请改为读取标签 / 属性 / 钩子上下文" ;;
  esac
done < <(grep -rnE --include=*.cpp --include=*.h "$R1_PATTERN" $SYSTEM_DIRS 2>/dev/null \
         | grep -v "/Components/CC_BuffComponent" | grep -v "/Tests/" | grep -v "/AbilitySystem/Effects/Buff/" | grep -vE ":[0-9]+:[[:space:]]*(//|\*)")

# ---------------------------------------------------------------------------
# 规则 4：敌我判断不写死在 Actor Tag 上；阵营系统完成后由 LEGACY 升级为 ERROR
while IFS= read -r HIT; do
  [ -z "$HIT" ] && continue
  allowed "$HIT" R4 && continue
  case "$HIT" in
    *"//"*FindClosestActorWithTag*|*"//"*TargetingActorTag*) ;;   # 注释行忽略
    */Tests/*|*/Data/CC_CharacterConfig.h*|*/Utils/*BlueprintLibrary*) ;;
    *)
      if [ -n "$TEAM_H" ]; then
        err R4 "$HIT  基于 Actor Tag 的敌我判断；请改用 UCC_TeamStatics::IsHostile"
      else
        legacy R4 "$HIT  基于 Actor Tag 的敌我判断（见 Roadmap 第 1 项，阵营系统完成后自动改为 ERROR）"
      fi ;;
  esac
done < <(grep -rnE "FindClosestActorWithTag\(|TargetingActorTag" "$SRC/Private" 2>/dev/null)

# ---------------------------------------------------------------------------
# 规则 5：Behavior 创建或订阅的一切必须能自动收回
# Behavior 子类不得直接订阅委托、开计时器、生成 Actor、施加 GE；
# 只能用基类的 ListenGameplayEvent / StartTimer / SpawnOwnedActor / ApplyChildEffect，
# 由 BuffComponent 在 OnDeactivated 后统一回收。
R5_PATTERN='AddUObject\(|AddDynamic\(|AddUniqueDynamic\(|AddLambda\(|AddWeakLambda\(|AddSP\(|SetTimer\(|SetTimerForNextTick\(|SpawnActor(Deferred)?[<(]|ApplyGameplayEffect(Spec)?To(Target|Self)\(|ApplyGameplayEffectSpecToTarget\('
for H in $BEHAVIOR_SUB_H; do
  BASE="$(basename "$H" .h)"
  for F in "$H" $(find "$SRC" -name "$BASE.cpp" -not -path "*/Tests/*" 2>/dev/null); do
    while IFS= read -r HIT; do
      [ -z "$HIT" ] && continue
      allowed "$HIT" R5 && continue
      [[ "$HIT" =~ ^[0-9]+:[[:space:]]*(//|\*) ]] && continue
      if [ "$HAS_OWNED_HELPERS" -eq 1 ]; then
        err R5 "$F:$HIT  Behavior 直接创建了订阅 / 计时器 / Actor / GE；请改用基类的自动回收工具"
      else
        legacy R5 "$F:$HIT  Behavior 直接创建资源；基类自动回收工具尚未实现，实现后此项变为 ERROR"
      fi
    done < <(grep -nE "$R5_PATTERN" "$F" 2>/dev/null)
  done
done

# ---------------------------------------------------------------------------
# 规则 6：共享配置不存实例状态
# (a) Buff 配置类（Definition / UIData / RulesComponent）不得有 mutable 成员。
# (b) Behavior 子类的 UPROPERTY 要么是策划配置（Edit*），要么标明 Transient 运行时状态，
#     避免把运行时数据序列化进模板。
for F in "$DEF_H" "$UIDATA_H" "$RULES_H"; do
  [ -f "$F" ] || continue
  while IFS= read -r HIT; do
    allowed "$HIT" R6 || err R6 "$F:$HIT  共享配置出现 mutable 成员；实例状态应放在 FCC_BuffInstance 或 Behavior 实例中"
  done < <(grep -nE "^[[:space:]]*mutable[[:space:]]" "$F" 2>/dev/null)
done
for H in $BEHAVIOR_SUB_H; do
  while IFS= read -r HIT; do
    [ -z "$HIT" ] && continue
    allowed "$HIT" R6 && continue
    err R6 "$H:$HIT  Behavior 的 UPROPERTY 既不是 Edit*（配置）也不是 Transient（运行时状态）"
  done < <(grep -nE "UPROPERTY\(" "$H" | grep -vE "Edit(DefaultsOnly|Anywhere|InstanceOnly)|Transient")
done

# ---------------------------------------------------------------------------
# 规则 7：钩子上下文的每个可修改字段必须写明多 Buff 同时修改时的合并方式
# 适用于名字以 HookContext / FireContext / Payload 结尾的结构体。
# 字段所在行或其上方注释需包含"合并："；"只读"注释之后、下一个"可修改"注释之前的字段不检查。
while IFS= read -r CTX_FILE; do
  [ -z "$CTX_FILE" ] && continue
  while IFS= read -r HIT; do
    [ -z "$HIT" ] && continue
    allowed "$HIT" R7 && continue
    err R7 "$CTX_FILE:$HIT  可修改字段缺少合并方式注释（例：// 合并：相加 / 相乘 / 取最大 / OR）"
  done < <(awk '
    /struct[ \t]+([A-Z_]+_API[ \t]+)?F[A-Za-z0-9_]*(HookContext|FireContext|Payload)([ \t:{]|$)/ {
      in_s = 1; depth = 0; started = 0; ro = 0; note = ""
    }
    in_s {
      line = $0
      o = gsub(/\{/, "{", line); c = gsub(/\}/, "}", line)
      depth += o - c; if (o > 0) started = 1
      if (line ~ /^[ \t]*(\/\/|\/\*|\*)/) {
        if (line ~ /只读/) ro = 1
        if (line ~ /可修改/) ro = 0
        note = note line
      } else if ((line ~ /;/ && line !~ /\(/) || line ~ /UPROPERTY\(.*\).*;/) {
        decl = line; sub(/.*UPROPERTY\(([^()]|\([^()]*\))*\)/, "", decl)
        if (decl ~ /;/ && decl !~ /GENERATED_BODY|static|using|friend|^[ \t]*\}/ && depth >= 1) {
          if (!ro && (note line) !~ /合并/) print NR ":" $0
          note = ""
        }
      } else if (line !~ /UPROPERTY\(/) note = ""
      if (started && depth <= 0) in_s = 0
    }' "$CTX_FILE")
done < <(grep -rlE --include=*.h "struct[[:space:]]+([A-Z_]+_API[[:space:]]+)?F[A-Za-z0-9_]*(HookContext|FireContext|Payload)\b" "$SRC" 2>/dev/null)

# ---------------------------------------------------------------------------
if [ "$MODE" = "hook" ]; then
  HITS="$(printf '%s' "$OUT" | grep "^ERROR" | grep -F -- "${HOOK_FILE}" || true)"
  if [ -n "$HITS" ]; then
    { echo "Buff 规则检查失败（详见 CLAUDE.md，确有理由可在行尾写 // harness:allow R<编号> <理由>）："; printf '%s\n' "$HITS"; } >&2
    exit 2
  fi
  exit 0
fi

printf '%s' "$OUT"
echo
echo "检查完成：ERROR $ERRORS 项，LEGACY $LEGACY 项"
[ "$ERRORS" -eq 0 ]
