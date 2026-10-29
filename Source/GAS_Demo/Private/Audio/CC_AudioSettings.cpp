#include "Audio/CC_AudioSettings.h"
#include "Audio/CC_AudioConfig.h"

UCC_AudioConfig* UCC_AudioSettings::GetAudioConfig()
{
	// 加载结果用 Root Set 保活，而不是存到 CDO 成员上（见头文件注释）。
	/**
	CDO 是 Class Default Object（类默认对象） 的缩写，是 Unreal 的一个基础概念。
	每个 UClass 都会自动生成一个"样板实例"，就是 CDO。
	比如你写了 UCC_AudioSettings 这个类，引擎启动时会自动 new 一个它的对象，名字叫 Default__CC_AudioSettings。
	崩溃日志里出现的就是这个名字。
	它不是你在游戏里创建的，而是引擎替每个类自动准备的。

	它有几个用途：
	保存默认值。
	你在构造函数里写 MaxHealth = 100，或者在蓝图 Class Defaults 面板里改的数值，
	其实都是写在 CDO 上的。
	当新对象的模板。 SpawnActor、NewObject 创建新对象时，会先从 CDO 复制一份属性，再跑你的初始化逻辑。
	不实例化就能读类的信息。
	GetDefault<UCC_AudioSettings>() 拿到的就是 CDO。
	所以项目设置类（UDeveloperSettings）都是这么用的：设置界面里填的值写进 CDO，
	代码里通过 GetDefault 读出来。全局只有这一份，所以很适合放配置。

	原代码里 GetMutableDefault<UCC_AudioSettings>() 拿到的是 CDO，
	然后把加载好的 DA_AudioConfig_Default 存到它的成员上，
	正好违反了这条约定。
	编辑器里没有这个特殊区域，所以平时运行没问题，打包后才暴露出来。
	 * 
	 */
	
	static TWeakObjectPtr<UCC_AudioConfig> RootedConfig;

	const UCC_AudioSettings* Settings = GetDefault<UCC_AudioSettings>();
	UCC_AudioConfig* Config = Settings->DefaultAudioConfig.LoadSynchronous();
	if (RootedConfig.Get() != Config)
	{
		// 编辑器里更换了默认配置：放开旧的，保活新的。
		if (UCC_AudioConfig* Previous = RootedConfig.Get())
		{
			Previous->RemoveFromRoot();
		}
		if (Config)
		{
			Config->AddToRoot();
		}
		RootedConfig = Config;
	}
	return Config;
}
