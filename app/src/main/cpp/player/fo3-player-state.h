#pragma once

#include "pipboy/fo3-pipboy-session.h"
#include "player/fo3-experience.h"
#include "scripting/fo3-event-scripts.h"
#include "world/fo3-reference-state.h"
#include "weapons/fo3-weapon-data.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fo3player {
constexpr uint32_t PlayerBase = 0x7u;
constexpr uint32_t PlayerRef = 0x14u;
enum class Special : uint8_t {
  Strength,
  Perception,
  Endurance,
  Charisma,
  Intelligence,
  Agility,
  Luck
};
// DNAM order; Throwing is an unused original slot, not New Vegas Survival.
enum class Skill : uint8_t {
  Barter,
  BigGuns,
  EnergyWeapons,
  Explosives,
  Lockpick,
  Medicine,
  MeleeWeapons,
  Repair,
  Science,
  SmallGuns,
  Sneak,
  Speech,
  Throwing,
  Unarmed
};
enum class ItemKind : uint8_t {
  Weapon,
  Armour,
  Ammo,
  Aid,
  Ingredient,
  Misc,
  Key,
  Book,
  Note
};

struct Item {
  fo3weapon::Definition weapon;
  uint32_t formId = 0, recordFlags = 0, bipedMask = 0, script = 0,
           enchantment = 0;
  ItemKind kind = ItemKind::Misc;
  std::string editorId, name, model, femaleModel, icon, femaleIcon;
  int32_t value = 0, maxCondition = 0;
  float weight = 0;
  uint8_t ingestibleFlags=0;
  int32_t equipType=-1;
  bool playable = true, questItem = false, cannotDrop = false,
       valueKnown = true;
};
// Original XOWN/XRNK or COED owner + rank/global union. A missing owner
// inherits CELL ownership. A null owner is not an ownership exemption.
struct OwnershipData {
  uint32_t owner=0, global=0;
  int32_t rank=0;
  bool explicitOwner=false, valid=true;
};
enum class OwnershipKind { Unowned, Player, NPC, Faction, Unknown };
struct OwnershipResult {
  OwnershipData data;
  OwnershipKind kind=OwnershipKind::Unowned;
  uint32_t cell=0;
  bool inherited=false, permitted=true, known=true, karmaEligible=false;
  bool Theft() const {return known && !permitted;}
};
enum class CrimeAction : uint8_t { Take, PickLock, ForceLock };
struct TheftEvent {
  CrimeAction action=CrimeAction::Take;
  uint32_t reference=0,item=0,container=0,owner=0,player=PlayerRef,cell=0,world=0;
  int32_t count=0;
  float karmaDelta=0;
  std::array<float,3> position{};
  bool positionKnown=false;
  // No fabricated witness/detection result; later crime perception consumes this.
};
struct KarmaEvent {float amount=0,requested=0;};
struct Stack {
  uint64_t id = 0;
  uint32_t formId = 0;
  int32_t count = 0;
  float condition =
      1; // normalized instance condition, never merged across conditions
  bool equipped = false;
  uint16_t loadedRounds = 0;
  bool needsAction = false;
  OwnershipData ownership{};
};
struct WorldWeapon {
  Stack instance;
  fo3weapon::WorldPose pose;
};
struct Rules {
  float healthEnduranceMult = 0, healthEnduranceOffset = 0, healthLevelMult = 0;
  float apBase = 0, apMult = 0, carryBase = 0, carryMult = 0;
};
enum class LockChange : uint8_t { Locked, Key, Picked, Forced, Broken, ScriptUnlocked };
struct LockOverride { LockChange change=LockChange::Locked; uint8_t level=0,forceFailures=0; bool rewarded=false; };
struct LockInfo { bool known=false,locked=false,broken=false,leveled=false; uint8_t level=0,flags=0,forceFailures=0; uint32_t key=0; };
enum class PickAccess { Ready, Unlocked, Unsupported, RequiresKey, Skill, NoPins, Broken };
struct Reference {
  uint32_t base = 0, flags = 0, owner = 0, key = 0, cell = 0;
  int32_t count = 1;
  float condition = 1;
  bool locked = false, valid = true;
  OwnershipData ownership{};
  uint8_t lockLevel=0,lockFlags=0;
  std::array<uint8_t,8> lockMetadata{};
};
struct LootEntry {
  uint32_t form = 0, owner = 0;
  int32_t count = 1;
  uint16_t level = 1;
  float condition = 1;
  bool extra = false;
  uint32_t ownerParameter=0;
};
struct Container {
  std::string name;
  uint32_t script = 0;
  bool valid = true, respawns = false;
  std::vector<LootEntry> entries;
};
struct LootList {
  uint8_t flags = 0, chanceNone = 0;
  uint32_t global = 0;
  bool valid = true;
  std::vector<LootEntry> entries;
};
// Scene-independent game coordinates. Nonresident actors freeze here rather
// than moving through unavailable geometry. No tracking/render pointers.
struct ActorState {
  uint32_t cell=0,world=0,package=0,sequence=0,hostile=0;
  std::array<float,3> position{};
  float yaw=0,packageWaitSeconds=0;
  uint64_t equippedWeapon=0;
  bool dead=false;
};
enum class QuestNoticeKind : uint8_t { Added, Updated, Completed };
struct ScriptItemNotice {uint32_t form=0;int32_t count=0;};
struct QuestNotice { uint32_t quest=0; QuestNoticeKind kind=QuestNoticeKind::Updated; };
struct ScriptMessage {
  uint32_t form=0, owner=0;
  std::string title,text;
  std::vector<std::string> buttons;
};
struct MerchantVisit {uint32_t day=0;float hour=0;};
struct State {
  fo3pipdata::SessionState pipboy;
  std::array<uint8_t, 7> special{};
  std::array<uint8_t, 14> skills{}, skillOffsets{};
  uint16_t level = 1;
  int32_t baseHealth = 0;
  float karma = 0, healthDamage = 0, apSpent = 0;
  // One authoritative 24-hour clock shared by lighting and authored AI PACKs.
  // GameDaysPassed tracks midnight boundaries; dated calendar gates remain unsupported.
  float gameHour = 12.0f;
  uint32_t gameDaysPassed = 0;
  uint32_t gameYear=2277,gameMonth=7,gameDay=17; // Original GLOB default: 17 August 2277 (GameMonth 0-based).
  uint32_t experience=0; // Canonical earned XP; level-up allocation remains a separate subsystem.
  std::unordered_map<uint32_t,LockOverride> locks;
  // Staged world state. Script Enable/Disable stays unavailable until scene
  // publication is integrated; this overlay is never a success-only command.
  std::unordered_map<uint32_t,bool> referenceEnabled;
  uint64_t referenceStateRevision=0;
  uint64_t nextStackId = 1;
  std::vector<Stack> inventory;
  std::unordered_set<uint32_t> collected;
  // Per-save SetQuestObject overrides; original ESM item metadata remains immutable.
  std::unordered_map<uint32_t,bool> questObjectFlags;
  std::unordered_map<uint32_t, std::vector<Stack>> containers;
  std::unordered_map<uint32_t,MerchantVisit> merchantVisits;
  std::unordered_map<uint32_t,int32_t> actorDispositionModifiers;
  std::unordered_map<uint32_t,uint32_t> cellNameMessages; // CELL -> original MESG FULL
  std::unordered_set<uint32_t> achievements; // local authored events, no platform unlock
  std::vector<WorldWeapon> worldWeapons;
  bool developmentWeaponGranted = false;
  std::unordered_map<uint32_t,float> actorDamage;
  std::unordered_map<uint32_t,ActorState> actors;
};
struct CellOwnership {OwnershipData data;uint8_t flags=0;std::string editor,name;};
// Sparse immutable XESP graph, independent of Pip-Boy target pruning.
struct EnableParent {uint32_t reference=0;uint8_t flags=0;bool valid=true;};
struct BarterRequest {uint32_t actor=0;int discount=0;};
struct BarterSelection {uint64_t stack=0;int32_t count=0;bool buying=false;};
struct BarterSession {
  uint32_t actor=0,container=0;int discount=0;
  const void* owner=nullptr;uint8_t skill=0;
  std::unordered_map<uint32_t,uint8_t> perks;
  std::vector<Stack> player,merchant;
  std::vector<BarterSelection> selections;
};
struct Catalog {
  fo3weapon::Definitions weapons;
  std::unordered_map<uint32_t,uint32_t> merchantContainers,merchantServices;
  std::unordered_map<std::string,float> barterSettings;
  uint32_t merchantRespawnHours=72; // Fallout3.exe iHoursToRespawnCell default.
  std::unordered_set<uint32_t> encounterZoneReferences;
  // Immutable original ACHR census. This is a source data catalogue, not a
  // request to build actor visuals or simulate every world CELL.
  std::unordered_map<uint32_t,fo3pipdata::Placement> actorPlacements;
  fo3pipdata::Definitions pipboy;
  // Derived once when the Player freezes its catalogue; never stores pointers
  // into copyable/sortable INFO vectors. A zero topic marks an invalid identity.
  std::unordered_map<uint32_t,std::pair<uint32_t,size_t>> dialogueInfos;
  std::unordered_map<uint32_t, Item> items;
  std::unordered_map<uint32_t, Reference> references;
  std::unordered_map<uint32_t,EnableParent> enableParents;
  // Authored base SCRI associations (actor/activator/door/other object).
  std::unordered_map<uint32_t,uint32_t> eventBaseScripts;
  std::unordered_map<uint32_t,std::string> eventBaseNames;
  std::unordered_map<uint32_t,fo3script::EventProgram> eventPrograms;
  std::unordered_set<uint32_t> doorBases;
  std::unordered_set<uint32_t> scriptedBases;
  std::unordered_set<uint32_t> defaultActivationDoors; // verified script passthroughs
  std::unordered_map<std::string, std::string> strings;
  std::unordered_map<uint32_t, Container> containers;
  std::unordered_map<uint32_t, Container> actorInventories;
  std::unordered_map<uint32_t, LootList> lootLists;
  std::unordered_map<uint32_t, float> globals;
  std::unordered_map<uint32_t,CellOwnership> cellOwnership;
  std::unordered_map<uint32_t,uint32_t> factionFlags;
  float theftKarma=0;
  uint32_t bobbyPin=0,infiltrator=0;
  std::unordered_map<std::string,float> lockSettings;
  uint32_t xpBumpBase = fo3experience::VanillaXPBump; // ESM iXPBumpBase GMST
  // FO3 executable defaults; optional GMST overrides. Not FNV reputation.
  float evilMaxKarma=-250;
  int32_t karmaMajorThreshold=250;
  float gameTimeScale = 30.0f; // Fallout 3 TimeScale GLOB, default 30 game min / real min.
  float npcRunMultiplier = 1.0f; // Optional original fMoveRunMult GMST; do not invent.
  float npcBaseSpeed = 0.f; // Original fMoveBaseSpeed, in game units/second.
  // Optional original GMSTs. Missing data disables the added morale/range policy.
  std::unordered_map<std::string,float> npcCombatSettings;
  uint32_t hourGlobal=0,dayGlobal=0,monthGlobal=0,yearGlobal=0,daysPassedGlobal=0,timeScaleGlobal=0;
  int32_t lootLevelDifference = 0;
  Rules rules;
  State initial;
  uint32_t fingerprint = 0;
  uint32_t worldFingerprint = 0;
  uint32_t lootFingerprint = 0;
};
// All-or-nothing, single-master Fallout3.esm loader. No plugins, scripts or
// save import.
bool LoadCatalog(const std::string &esmPath, Catalog &out, std::string &error);

struct ActorCensusReport {
  size_t authored=0,registered=0,alreadyTracked=0,disabledOrConditional=0;
  size_t unsupportedBase=0,unsupportedPlacement=0,limitReached=0;
};
enum class ScriptEventResult : uint8_t { NoHandler, Executed, Unsupported, Pending };
class Player;
class ScriptTransactionPlan;
enum class ScriptProgramKind : uint8_t {UnboundResult,QuestStageItem,ReferenceEvent,DialogueInfo};
enum class DialogueScriptPhase : uint8_t {Begin,End,Both};
enum class ScriptProgramCapture : uint8_t {None,EnteredPrograms};
struct ScriptProgramOrigin {
  ScriptProgramKind kind=ScriptProgramKind::UnboundResult;
  uint32_t record=0,item=0,sourceLine=0,topic=0;
  uint16_t stage=0;
  std::string event;
};
// Immutable evidence of entered programs, not authority to admit world changes.
// Unbound dialogue bodies deliberately have no invented INFO form identity.
struct ScriptVariableBinding {
  uint32_t owner=0,script=0,index=0,base=0;
  bool quest=false;
  std::string ownerName,variableName;
  bool operator==(const ScriptVariableBinding& b) const noexcept {
    return owner==b.owner&&script==b.script&&index==b.index&&base==b.base&&quest==b.quest&&
      ownerName==b.ownerName&&variableName==b.variableName;
  }
};
struct ScriptExecution {
  static constexpr uint32_t NoParent=UINT32_MAX;
  ScriptProgramOrigin origin;
  uint32_t owner=0,activator=0,localScript=0;
  // Entry-order parent index and the inherited implicit reference used by
  // distance/button/message operations. These are not the quest local owner.
  uint32_t parent=NoParent,reference=0;
  fo3pipdata::ResultScript program,container;
  // Decoded authored gate metadata, not condition outcomes or read ownership.
  // QUST entries retain item conditions; INFOs also retain their parent gates.
  uint32_t owningQuest=0; // INFO QSTI only
  std::vector<fo3pipdata::Condition> conditions,questConditions;
  // Canonical index order; preserves decoded names rather than map iteration.
  std::vector<std::pair<uint32_t,std::string>> localVariables;
  // Successful command-compiler form-name lookups, in lookup order (including
  // unreachable commands). Not expression or qualified-variable read evidence.
  std::vector<std::pair<uint32_t,std::string>> commandForms;
  // Successful form-name lookups in the all-expression preflight, in lookup
  // order. This is binding identity, not runtime read values or branch outcomes.
  std::vector<std::pair<uint32_t,std::string>> expressionForms;
  std::vector<ScriptVariableBinding> commandVariables,expressionVariables;
  // Successful bare NumericLocalKey lookups. Owner/script come from this entry;
  // pairs retain the resolved slot and normalized token in lookup order.
  std::vector<std::pair<uint32_t,std::string>> commandLocals,expressionLocals;
  // One indexed binding per CTDA 53/79 in authored list order. ownerName is
  // empty: these conditions use FormIDs, not editor-name resolution. These
  // describe potential gate dependencies, not evaluated reads or outcomes.
  std::vector<ScriptVariableBinding> conditionVariables,questConditionVariables;
  // Potential CTDA 74 and flags&4 dependencies, ordered value query then
  // comparison per condition. Each pair is (FormID, immutable catalog-default
  // float bits), never a mutable runtime value or a condition outcome.
  std::vector<std::pair<uint32_t,uint32_t>> conditionGlobals,questConditionGlobals;
  // Successful compiled global assignment targets and expression-preflight
  // global lookups, using the same immutable default-bit representation.
  std::vector<std::pair<uint32_t,uint32_t>> commandGlobals,expressionGlobals;
};
struct ReferenceEnableChange { uint32_t reference=0; bool enabled=false; };
// Owned reference-only input for the future scene coordinator. It neither runs
// scripts nor publishes world resources. Workers capture the immutable snapshots,
// never this render-thread-owned plan or mutable Player state.
class ReferenceEnablePlan {
public:
  ReferenceEnablePlan(const ReferenceEnablePlan&)=delete;
  ReferenceEnablePlan& operator=(const ReferenceEnablePlan&)=delete;
  bool HasChanges() const { return changed_; }
  const std::vector<uint32_t>& AffectedReferences() const { return affected_; }
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> CandidateSnapshot() const { return candidate_; }
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> RollbackSnapshot() const { return rollback_; }
private:
  friend class Player;
  ReferenceEnablePlan()=default;
  enum class Phase : uint8_t { Prepared, Applied, RolledBack };
  const Player* owner_=nullptr;
  std::shared_ptr<const uint8_t> ownerIdentity_;
  uint64_t expectedRevision_=0,expectedReferenceRevision_=0;
  bool changed_=false;
  Phase phase_=Phase::Prepared;
  std::unordered_map<uint32_t,bool> roots_;
  std::vector<uint32_t> affected_;
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> candidate_,rollback_;
};
class Player {
public:
  explicit Player(Catalog catalog);
  // Idempotent, bounded source-record registration; only initial/root state,
  // never replaces an existing actor's saved cell, health or package.
  ActorCensusReport RegisterOriginalActors();
  const Catalog &Definitions() const { return *catalog_; }
  const State &Snapshot() const { return state_; }
  fo3experience::Progress ExperienceProgress() const {
    return fo3experience::Evaluate(state_.level,state_.experience,catalog_->xpBumpBase);
  }
  float MaxHealth() const;
  float Health() const;
  float MaxActionPoints() const;
  float ActionPoints() const;
  float CarryCapacity() const;
  double InventoryWeight() const;
  bool Overencumbered() const { return InventoryWeight() > CarryCapacity(); }
  uint64_t Revision() const { return revision_; }
  // Mutable player attributes are authoritative save state, not ESM edits.
  // SPECIAL 1..10, base skill ratings 0..100 (including unused Throwing slot).
  bool SetPlayerAttributes(const std::array<uint8_t,7>& special,
                           const std::array<uint8_t,14>& skills);
  // Persisted motion/procedure progress still dirties saves, but is sampled
  // by AI's cadence rather than invalidating every actor's conditions per step.
  uint64_t PackageRevision() const { return revision_ - actorPoseRevisions_; }
  // The caller owns pause/loading policy; this mutates only canonical game time.
  bool AdvanceGameClock(double realSeconds);
  // Mutable engine GLOB values must not resolve through immutable ESM defaults.
  bool GlobalValue(uint32_t form,float& value) const;
  // Invalid requests are atomic no-ops. Removal respects quest/cannot-drop
  // flags.
  bool RecordDialogue(uint32_t actor,uint32_t info,const std::vector<uint32_t>& topics);
  bool SetDialogueVariable(uint64_t key,float value);
  bool ExecuteDialogueResult(const fo3pipdata::ResultScript& script,std::string& error,uint32_t speaker=0);
  // Evaluate against a private Player candidate sharing the immutable catalogue.
  // No live state/queues/callbacks change during preparation or previews.
  std::unique_ptr<ScriptTransactionPlan> PrepareDialogueTransaction(
      const fo3pipdata::ResultScript& begin,const fo3pipdata::ResultScript& end,std::string& error,uint32_t speaker=0,
      ScriptProgramCapture capture=ScriptProgramCapture::None) const;
  // Resolve the bodies from this Player's immutable canonical INFO catalogue.
  // Identity evidence only: this does not evaluate dialogue eligibility.
  std::unique_ptr<ScriptTransactionPlan> PrepareDialogueInfoTransaction(
      uint32_t topic,uint32_t info,DialogueScriptPhase phase,std::string& error,uint32_t speaker=0,
      ScriptProgramCapture capture=ScriptProgramCapture::None) const;
  std::unique_ptr<ScriptTransactionPlan> PrepareQuestStageTransaction(
      uint32_t quest,uint16_t stage,std::string& error,ScriptProgramCapture capture=ScriptProgramCapture::None) const;
  std::unique_ptr<ScriptTransactionPlan> PrepareReferenceEventTransaction(
      uint32_t reference,const char* event,uint32_t activator,std::string& error,
      ScriptProgramCapture capture=ScriptProgramCapture::None) const;
  bool CanCommitScriptTransaction(const ScriptTransactionPlan& plan) const noexcept;
  bool CanRollbackScriptTransaction(const ScriptTransactionPlan& plan) const noexcept;
  bool CanFinalizeScriptTransaction(const ScriptTransactionPlan& plan,bool committed) const noexcept;
  bool CommitScriptTransaction(ScriptTransactionPlan& plan) noexcept;
  bool RollbackScriptTransaction(ScriptTransactionPlan& plan) noexcept;

  // Event scripts dispatch through the same bounded transactional interpreter.
  // A rejected event does not roll back the preceding physical hit/activation.
  ScriptEventResult DispatchReferenceEvent(uint32_t reference,const char* event,
                                            uint32_t activator,std::string& error);
  bool HasReferenceEvent(uint32_t reference,const char* event) const;
  void PumpQuestGameMode(double realSeconds);
  void PumpReferenceGameMode(double realSeconds,const std::vector<uint32_t>& resident);
  // Native shared 4 Hz quest/reference scheduler. Only a reference-changing
  // prepared plan is handed to the resident-world coordinator. The caller
  // must own it until committed, rolled back or cancelled. Never run a second
  // evaluation to capture provenance.
  bool PumpWorldAwareGameMode(double realSeconds,const std::vector<uint32_t>& resident,
      const std::function<bool(std::unique_ptr<ScriptTransactionPlan>)>& reserveWorld);
  void SetScriptPlayerLocation(float x,float y,float z,uint32_t cell,uint32_t world);
  bool ScriptDistance(float& distance) const;
  uint32_t ScriptActivator() const { return scriptActivator_; }
  uint32_t ScriptReference() const { return scriptReference_; }
  int ScriptButton() const;
  int32_t ItemCount(uint32_t form) const;
  bool ScriptRemoveItem(uint32_t form,int32_t count);
  bool IsQuestObject(uint32_t form) const;
  bool SetQuestObject(uint32_t form,bool quest);
  bool RewardKarma(float amount);
  bool RewardExperience(uint32_t amount);
  bool CompleteAllObjectives(uint32_t quest);
  bool SetCellFullName(uint32_t cell,uint32_t message);
  std::string CellFullName(uint32_t cell) const;
  bool AddAchievement(uint32_t id);
  bool ScriptInSameCell(uint32_t reference,bool& result,std::string& error) const;
  bool ReevaluatePackage(uint32_t reference);
  // Returns false for an unknown/deleted reference or malformed/cyclic graph.
  bool ReferenceDisabled(uint32_t reference,bool& disabled,std::string& error) const;
  bool StageReferenceEnable(uint32_t reference,bool enabled,std::string& error);
  fo3world::ReferenceStateSnapshot CaptureReferenceState() const;
  // Pure planning: validate the entire ordered batch and precompute both sparse
  // projections without changing live state/revisions, save data or effect queues.
  std::unique_ptr<ReferenceEnablePlan> PrepareReferenceEnablePlan(
      const std::vector<ReferenceEnableChange>& changes,std::string& error) const;
  bool CanCommitReferenceEnablePlan(const ReferenceEnablePlan& plan) const noexcept;
  // Reference overlay only. The outer scene coordinator must preflight/exchange
  // every affected resource before calling this; source handlers stay unavailable.
  // Publication and immediate rollback allocate nothing. A newer Player mutation
  // rejects a stale plan. Rollback restores values but advances revisions to avoid
  // re-admitting work from an earlier publication. Destroying a plan finalizes it.
  bool CommitReferenceEnablePlan(ReferenceEnablePlan& plan) noexcept;
  bool RollbackReferenceEnablePlan(ReferenceEnablePlan& plan) noexcept;
  bool QueueScriptMessage(uint32_t form,float argument,bool hasArgument,uint32_t owner);
  bool QueueScriptSound(uint32_t form);
  bool PollScriptMessage(ScriptMessage& out);
  bool PollQuestNotice(QuestNotice& out);
  bool PollScriptItemNotice(ScriptItemNotice& out);
  size_t PendingScriptMessageCount() const {return pendingScriptMessages_.size();}
  bool SubmitScriptMessageButton(uint32_t owner,int index);
  bool PollScriptSound(uint32_t& out);
  void SetScriptEventDiagnostic(std::function<void(const std::string&)> diagnostic) {
    eventDiagnostic_=std::move(diagnostic);
  }
  bool PreviewDialogueResult(const fo3pipdata::ResultScript&,std::string&,uint32_t speaker=0) const;
  bool PreviewDialogueResults(const fo3pipdata::ResultScript&,const fo3pipdata::ResultScript&,std::string&,uint32_t speaker=0) const;
  bool ExecuteQuestStage(uint32_t,uint16_t,std::string&);
  bool StopQuest(uint32_t);
  bool SetGlobalValue(uint32_t,float);
  bool GrantPerk(uint32_t id, uint8_t rank);
  bool StartQuest(uint32_t id);
  bool SetQuestStage(uint32_t id,uint16_t stage);
  bool SetObjective(uint32_t id,uint32_t index,bool displayed,fo3pipdata::Completion status);
  bool FinishQuest(uint32_t id,fo3pipdata::Completion status);
  bool SelectQuest(uint32_t id);
  bool Discover(uint32_t id);
  bool SetWaypoint(uint32_t world,float x,float y);
  bool TuneRadio(uint32_t reference);
  bool CanUse(uint64_t stack,std::string* reason=nullptr) const;
  bool Use(uint64_t stack);
  bool Add(uint32_t formId, int32_t count, float condition = 1);
  bool Remove(uint64_t stackId, int32_t count);
  bool Equip(uint64_t stackId);
  bool Unequip(uint64_t stackId);
  const Stack *Weapon(uint64_t id) const;
  const Stack *EquippedWeapon() const;
  int32_t AmmoReserve(uint32_t ammo) const;
  bool PickupWeapon(uint32_t reference);
  bool DropWeapon(uint64_t id, const fo3weapon::WorldPose &pose);
  bool PickupWorldWeapon(uint64_t id);
  bool UpdateWorldWeapon(uint64_t id, const fo3weapon::WorldPose &pose);
  bool FireWeapon(uint64_t id);
  bool EjectMagazine(uint64_t id);
  bool LoadMagazine(uint64_t id);
  bool ChamberWeapon(uint64_t id);
  // Explicit development bootstrap; runtime enables only when presentation is ready.
  bool BootstrapDevelopmentWeapon();
  float WeaponDamage(uint64_t instance) const;
  float ActorHealth(uint32_t reference) const;
  bool WeaponHit(uint64_t instance,uint32_t target,float fraction=1);
  bool ApplyWeaponHit(uint32_t base,uint32_t target,float damage);
  bool ApplyAttack(uint32_t attacker,uint32_t weapon,uint32_t target,float damage);
  bool Essential(uint32_t reference) const;
  bool PrepareActorInventory(uint32_t reference);
  bool EquipActorWeapon(uint32_t reference,uint64_t instance);
  bool FireActorWeapon(uint32_t reference,uint64_t instance);
  bool ReloadActorWeapon(uint32_t reference,uint64_t instance);
  const Stack* ActorWeapon(uint32_t reference) const;
  float ActorWeaponDamage(uint32_t reference) const;
  float DamageResistance(uint32_t reference) const;
  bool UpdateActor(uint32_t reference,const ActorState& state);
  bool SetActorHostile(uint32_t reference,uint32_t target);
  OwnershipResult ResolveOwnership(uint32_t reference, const OwnershipData* item=nullptr) const;
  OwnershipResult ContainerStackOwnership(uint32_t reference,uint64_t stack) const;
  void BeginContainerInteraction(uint32_t reference);
  void EndContainerInteraction();
  bool PollTheftEvent(TheftEvent& out);
  bool PollKarmaEvent(KarmaEvent& out);
  bool CanPickup(uint32_t reference) const;
  bool Pickup(uint32_t reference);
  LockInfo GetLock(uint32_t reference) const;
  int LockRequirement(uint32_t reference) const;
  PickAccess CanPickLock(uint32_t reference) const;
  int ForceLockChance(uint32_t reference) const;
  bool UnlockByKey(uint32_t reference);
  bool CompleteLockpick(uint32_t reference,bool forced=false);
  bool ForceLock(uint32_t reference,uint32_t roll);
  bool BreakBobbyPin(uint32_t reference);
  bool ScriptLock(uint32_t reference,int level);
  bool ScriptUnlock(uint32_t reference);
  float LockSetting(const std::string& key,float fallback) const;
  bool CanOpenDoor(uint32_t reference) const;
  bool CanActorOpenDoor(uint32_t actor,uint32_t reference) const;
  bool CanLootContainer(uint32_t reference) const;
  bool PrepareContainer(uint32_t reference);
  bool MerchantAvailable(uint32_t actor) const;
  bool OpenBarter(uint32_t actor,int discount,BarterSession& out);
  bool BarterItemAllowed(uint32_t actor,const Stack& item) const;
  bool BarterPrice(const Stack& item,bool buying,int discount,float& out) const;
  bool BarterBalance(const BarterSession&,int64_t& net,std::string& error) const;
  bool ConfirmBarter(BarterSession&,std::string& error);
  bool QueueBarter(uint32_t actor,int discount);
  bool ModActorDisposition(uint32_t actor,int32_t amount);
  bool PollBarter(BarterRequest& out);
  const std::vector<Stack> *ContainerContents(uint32_t reference) const;
  bool CanTakeContainerStack(uint32_t reference, uint64_t stack) const;
  bool TakeContainerStack(uint32_t reference, uint64_t stack);
  bool IsCollected(uint32_t reference) const {
    return state_.collected.count(reference) != 0;
  }
  bool DamageHealth(float amount);
  bool RestoreHealth(float amount);
  bool SpendActionPoints(float amount);
  bool RestoreActionPoints(float amount);
  // Versioned Quest state, NOT a Bethesda .fos. Restore never partially changes
  // state.
  bool Save(const std::string &path, std::string &error) const;
  bool Restore(const std::string &path, std::string &error);

private:
  struct ScriptCandidateTag {};
  Player(const Player& source,ScriptCandidateTag);
  std::unique_ptr<ScriptTransactionPlan> PrepareScriptTransaction(
      const std::function<bool(Player&,std::string&)>& execute,std::string& error,bool capturePrograms=false) const;
  void SwapScriptPayload(Player& other) noexcept;
  bool ScriptPlanContextMatches(const ScriptTransactionPlan& plan) const noexcept;
  bool RunSourceEventDirect(uint32_t scriptId,const char* event,uint32_t owner,
                            uint32_t activator,std::string& error);
  bool ReferenceDisabledWith(const std::unordered_map<uint32_t,bool>& roots,
                            uint32_t reference,bool& disabled,std::string& error) const;
  bool StageReferenceEnableIn(std::unordered_map<uint32_t,bool>& roots,
                             uint32_t reference,bool enabled,bool& changed,std::string& error) const;
  fo3world::ReferenceStateSnapshot CaptureReferenceStateWith(
      const std::unordered_map<uint32_t,bool>& roots,uint64_t revision) const;
  std::shared_ptr<const uint8_t> referencePlanIdentity_=std::make_shared<const uint8_t>(0);
  void CommitTheft(uint32_t reference,uint32_t item,int32_t count,uint32_t container,const OwnershipResult& ownership);
  bool RunSourceEvent(uint32_t scriptId,const char* event,uint32_t owner,
                      uint32_t activator,std::string& error);
  void LogScriptEventError(uint32_t scriptId,const char* event,const std::string& error);
  bool PrepareInventory(uint32_t reference,bool actor);
  void TouchMerchantCells(uint32_t cell);
  bool RestockMerchant(uint32_t actor,uint32_t chest);
  bool MigrateWeaponInstances(State &state) const;
  bool RunScript(const fo3pipdata::ResultScript&,uint32_t owner,unsigned depth,unsigned& budget,std::string&,uint32_t scriptId=0,ScriptProgramOrigin origin={});
  bool RunQuestStage(uint32_t,uint16_t,unsigned depth,unsigned& budget,std::string&);
  bool EnsureQuestInstance(uint32_t);
  std::shared_ptr<const Catalog> catalog_;
  mutable State state_;
  mutable uint64_t revision_ = 0;
  uint64_t actorPoseRevisions_ = 0;
  double questScriptAccumulator_=0,referenceScriptAccumulator_=0;
  uint32_t scriptActivator_=0,scriptReference_=0,scriptPlayerCell_=0,scriptPlayerWorld_=0;
  std::array<float,3> scriptPlayerPosition_{};
  bool scriptPlayerPositionKnown_=false;
  uint64_t scriptEffectRevision_=0;
  std::vector<ScriptExecution>* scriptTrace_=nullptr; // Private preparation only; never published/swapped.
  uint32_t scriptTraceParent_=ScriptExecution::NoParent;
  size_t scriptTraceBytes_=0;
  std::unordered_map<uint32_t,int> scriptButtons_;
  std::vector<ScriptMessage> pendingScriptMessages_;
  std::vector<QuestNotice> pendingQuestNotices_;
  std::vector<ScriptItemNotice> pendingScriptItems_;
  std::vector<uint32_t> pendingScriptSounds_;
  std::vector<BarterRequest> pendingBarters_;
  std::vector<KarmaEvent> pendingKarmaEvents_;
  std::vector<TheftEvent> pendingTheftEvents_;
  uint32_t containerInteraction_=0;
  bool containerKarmaCommitted_=false;
  std::function<void(const std::string&)> eventDiagnostic_;
  std::unordered_set<std::string> reportedScriptEvents_;
};

// Candidate state is read-only; queued effects become visible only on commit.
// All preparation/publication belongs to the render thread. Workers receive
// owned reference snapshots, not the candidate Player or this mutable plan.
class ScriptTransactionPlan {
public:
  ScriptTransactionPlan(const ScriptTransactionPlan&)=delete;
  ScriptTransactionPlan& operator=(const ScriptTransactionPlan&)=delete;
  // Valid only while prepared; applied plans instead hold the displaced payload.
  const State* CandidateState() const {return phase_==Phase::Prepared?&candidate_->Snapshot():nullptr;}
  bool HasReferenceChanges() const {return references_&&references_->HasChanges();}
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> CandidateReferences() const {
    return references_?references_->CandidateSnapshot():nullptr;
  }
  std::shared_ptr<const fo3world::ReferenceStateSnapshot> RollbackReferences() const {
    return references_?references_->RollbackSnapshot():nullptr;
  }
  const std::vector<uint32_t>& AffectedReferences() const {
    return references_?references_->AffectedReferences():emptyAffected_;
  }
  const std::vector<ScriptExecution>& ExecutedScripts() const noexcept {return executions_;}
  bool HasProgramProvenance() const noexcept {return programsCaptured_;}
private:
  friend class Player;
  ScriptTransactionPlan()=default;
  enum class Phase : uint8_t {Prepared,Applied,RolledBack};
  Phase phase_=Phase::Prepared;
  const Player* owner_=nullptr;
  std::shared_ptr<const uint8_t> ownerIdentity_;
  uint64_t expectedRevision_=0,expectedEffects_=0;
  std::unique_ptr<Player> candidate_;
  std::unique_ptr<ReferenceEnablePlan> references_;
  std::vector<uint32_t> emptyAffected_;
  std::vector<ScriptExecution> executions_;
  bool programsCaptured_=false;
};

// Native integration owns one session for the application, across CELL/GL
// lifetimes. Access and mutations belong to the render thread; initial loading
// belongs to its worker.
struct Session {
  explicit Session(Catalog catalog) : player(std::move(catalog)) {}
  Player player;
  std::string savePath;
  bool saveBlocked = false;
  uint64_t savedRevision = UINT64_MAX;
  bool Flush(std::string &error);
};
} // namespace fo3player

// Null until the first scene publishes. No mutable global state exposed to
// workers.
fo3player::Session *GetFo3PlayerSession();
