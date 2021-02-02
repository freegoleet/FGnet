#include "FGReplicatorComponent.h"
#include "Engine/ActorChannel.h"
#include "FGReplicatorBase.h"
#include "GameFramework/Actor.h"

UFGReplicatorComponent::UFGReplicatorComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UFGReplicatorComponent::ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	WroteSomething |= Channel->ReplicateSubobjectList(SmoothReplicators, *Bunch, *RepFlags);

	return WroteSomething;
}

UFGReplicatorBase* UFGReplicatorComponent::AddReplicatorByClass(TSubclassOf<UFGReplicatorBase> ClassType, FName Name)
{
	// Can't have name as "None" since UObject creation will change it to class name instead.
	if (!ensureMsgf(!Name.IsNone(), TEXT("Replicator name cannot be None for: %s."), *GetOwner()->GetName()))
		return nullptr;

	if (!ensureMsgf(Name.IsValid(), TEXT("Replicator name is not valid for: %s."), *GetOwner()->GetName()))
		return nullptr;

	if (!ensureMsgf(!DoesReplicatorExist(Name), TEXT("A Replicator with the name %s already exists."), *Name.ToString()))
		return nullptr;

	UFGReplicatorBase* NewReplicator = NewObject<UFGReplicatorBase>(GetOwner(), ClassType, Name);
	NewReplicator->Init();
	SmoothReplicators.Add(NewReplicator);
	return NewReplicator;
}

UFGReplicatorBase* UFGReplicatorComponent::GetReplicatorByName(FName Name) const
{
	for (UFGReplicatorBase* ReplicatorInstance : SmoothReplicators)
	{
		if (ReplicatorInstance == nullptr)
			continue;

		if (ReplicatorInstance->GetFName() == Name)
			return ReplicatorInstance;
	}

	return nullptr;
}

bool UFGReplicatorComponent::DoesReplicatorExist(FName Name) const
{
	for (UFGReplicatorBase* ReplicatorInstance : SmoothReplicators)
	{
		if (ReplicatorInstance == nullptr)
			continue;

		const FName ReplicatorName = ReplicatorInstance->GetFName();

		if (ReplicatorName == Name)
			return true;
	}

	return false;
}