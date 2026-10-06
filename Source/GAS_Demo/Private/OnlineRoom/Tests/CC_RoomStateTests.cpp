#include "OnlineRoom/CC_OnlineRoomSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

/** 内存后端替身：让状态回归测试不依赖网卡、编辑器 PIE 或在线账号。 */
class FCC_RoomTestBackend final : public ICC_RoomSessionBackend
{
public:
	/** 测试只关心状态决策，无需获取 OSS。 */
	virtual void Initialize(UGameInstance*) override {}
	/** 替身没有外部资源。 */
	virtual void Shutdown() override {}
	/** 异步命令保持挂起，由测试直接投递完成结果。 */
	virtual bool HostRoom(const FCC_RoomCreateRequest&) override { return true; }
	virtual bool FindRooms(const FCC_RoomSearchRequest&) override { return true; }
	virtual bool JoinRoom(const FString&) override { return true; }
	virtual bool LeaveRoom() override { return true; }
	/** 模拟失败之后平台上仍然存在的本地 Session。 */
	virtual bool HasSession() const override { return bSession; }
	/** 暴露替身回调，与真实适配器遵守同一契约。 */
	virtual FOnOperationComplete& OnOperationComplete() override { return Operation; }
	virtual FOnSearchComplete& OnSearchComplete() override { return Search; }
	virtual FOnJoinComplete& OnJoinComplete() override { return Join; }
	bool bSession = false;
private:
	FOnOperationComplete Operation;
	FOnSearchComplete Search;
	FOnJoinComplete Join;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCRoomStateTest, "GAS_Demo.OnlineRoom.State.RejectionAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 验证重复按钮、事件重入及残留 Session 三条故障路径不会破坏活动事务。 */
bool FCCRoomStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UCC_OnlineRoomSubsystem* Rooms = NewObject<UCC_OnlineRoomSubsystem>(GI);
	Rooms->Backend = MakeUnique<FCC_RoomTestBackend>();
	auto* Backend = static_cast<FCC_RoomTestBackend*>(Rooms->Backend.Get());
	TestTrue(TEXT("Host begins"), Rooms->BeginOperation(ECC_RoomOperation::Host, ECC_RoomAsyncState::Creating));
	TestFalse(TEXT("Repeated Host rejected"), Rooms->BeginOperation(ECC_RoomOperation::Host, ECC_RoomAsyncState::Creating));
	TestEqual(TEXT("Rejected request preserves creating state"), Rooms->GetState(), ECC_RoomAsyncState::Creating);
	TestEqual(TEXT("Rejected request preserves active operation"), Rooms->ActiveOperation, ECC_RoomOperation::Host);
	Rooms->Reject(ECC_RoomOperation::Join, ECC_RoomResultCode::InvalidRequest, TEXT("Missing id"));
	TestEqual(TEXT("Invalid request also preserves Host"), Rooms->ActiveOperation, ECC_RoomOperation::Host);
	Backend->bSession = true;
	Rooms->FinishOperation(ECC_RoomOperation::Host,
		FCC_RoomOperationResult::Failure(ECC_RoomResultCode::TravelFailure, FText::GetEmpty()));
	TestEqual(TEXT("Failed travel with session requires cleanup"), Rooms->GetState(), ECC_RoomAsyncState::RecoveryRequired);
	TestFalse(TEXT("Join blocked until cleanup"), Rooms->BeginOperation(ECC_RoomOperation::Join, ECC_RoomAsyncState::Joining));
	TestTrue(TEXT("Leave allowed for recovery"), Rooms->BeginOperation(ECC_RoomOperation::Leave, ECC_RoomAsyncState::Destroying));
	Backend->bSession = false;
	Rooms->FinishOperation(ECC_RoomOperation::Leave, FCC_RoomOperationResult::Success());
	Rooms->bDispatching = true;
	TestFalse(TEXT("Observer cannot synchronously reenter"), Rooms->BeginOperation(ECC_RoomOperation::Search, ECC_RoomAsyncState::Searching));
	Rooms->bDispatching = false;
	TestEqual(TEXT("Idle remains intact"), Rooms->GetState(), ECC_RoomAsyncState::Idle);
	Rooms->Backend.Reset();
	return true;
}
#endif
