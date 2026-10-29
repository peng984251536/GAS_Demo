#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CC_ToastTypes.generated.h"

class UTexture2D;

/** 提示的视觉风格：决定强调色，蓝图可按它切换图标或动画。 */
UENUM(BlueprintType)
enum class ECC_ToastStyle : uint8
{
	Info,     // 普通信息（蓝）
	Success,  // 正向：升级、复活（绿/金）
	Warning,  // 注意：波次、敌人来袭（橙）
	Danger    // 负向：队友倒下（红）
};

/** 对局消息与本地玩家的关系过滤（用于 Target 为某个玩家的消息）。 */
UENUM(BlueprintType)
enum class ECC_ToastTargetFilter : uint8
{
	Anyone,      // 所有人都显示
	OnlyOthers,  // Target 是自己时不显示（自己倒下由复活倒计时负责）
	OnlySelf     // 只有 Target 是自己时显示
};

/** 提示条目在宿主中的阶段。 */
UENUM(BlueprintType)
enum class ECC_ToastPhase : uint8
{
	Entering,
	Showing,
	Leaving
};

/** 一条提示请求。BP 里 Make 这个结构后调用 CC Toast Statics 的 Show Toast；只要文字时用 Show Toast Text。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_ToastRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FText Title;

	/** 副标题/正文，可空。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	ECC_ToastStyle Style = ECC_ToastStyle::Info;

	/** 完全显示的秒数（不含进出场动画）；<=0 使用宿主默认值。按真实时间计时。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	float Duration = 0.f;

	/** 排队时数值大的先出；同优先级按到达顺序。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	int32 Priority = 0;

	/**
	 * 合并键：非空时，与正在显示或排队中的同键提示合并——更新内容并重新计时，不新增一条。
	 * 例如连续升级只保留一条"等级提升至 Lv.N"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FName DedupeKey;

	/** 来源标签（如 Survival.Msg.LevelUp），供条目蓝图切换图标、动画；系统不按它做逻辑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FGameplayTag SourceTag;

	/** 可选图标；条目里有 Image_Icon 时显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 真正显示时播放的音效事件（CC Audio Config 中的事件名），空则不播。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName SoundEvent;
};

/**
 * 可配置的提示模板（项目设置 → Game → CC Toast）。
 * Title / Message 支持命名参数：{Value}（消息数值，取整）、{Player}（Target 玩家名）、{Wave}（下一波序号）。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_ToastTemplate
{
	GENERATED_BODY()

	/** 关掉即不显示这条消息（用来屏蔽内置默认提示）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	ECC_ToastStyle Style = ECC_ToastStyle::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	ECC_ToastTargetFilter TargetFilter = ECC_ToastTargetFilter::Anyone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	FName DedupeKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Toast", meta=(GetOptions="CC_AudioLibrary.GetAudioEventNames"))
	FName SoundEvent;

	/** 用命名参数填充文本，生成一条请求。 */
	FCC_ToastRequest ToRequest(const FFormatNamedArguments& Args, FGameplayTag InSourceTag) const;
};

/**
 * 纯数据的提示队列：优先级排序、合并键去重、容量上限。不依赖 UObject，便于单元测试。
 */
struct GAS_DEMO_API FCC_ToastQueue
{
	/** 入队。同 DedupeKey 已在队列中时原地替换内容（保留排队位置）并返回 true。
	 *  超出容量时丢弃优先级最低、到达最早的一条。 */
	bool Push(const FCC_ToastRequest& Request, int32 MaxSize);
	/** 取出下一条（优先级最高、最早到达）。 */
	bool Pop(FCC_ToastRequest& OutRequest);
	bool IsEmpty() const { return Items.IsEmpty(); }
	int32 Num() const { return Items.Num(); }
	void Reset() { Items.Reset(); }
	const FCC_ToastRequest& Peek(int32 Index) const { return Items[Index].Request; }

private:
	struct FItem
	{
		FCC_ToastRequest Request;
		uint64 Serial = 0;
	};
	/** 始终按 (Priority 降序, Serial 升序) 排好。 */
	TArray<FItem> Items;
	uint64 NextSerial = 0;
};
