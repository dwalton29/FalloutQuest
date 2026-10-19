"""v230 non-behavioural CPU cache/ordering regression checks."""
from pathlib import Path
import unittest
BASE=Path(__file__).resolve().parents[2]
LOOP=(BASE/"app/src/main/cpp/core/fo3-runtime-loop.inc").read_text()
CORE=(BASE/"app/src/main/cpp/core/fo3-runtime.cpp").read_text()
SCRIPT=(BASE/"app/src/main/cpp/scripting/fo3-script-runtime.inc").read_text()
PLAYER=(BASE/"app/src/main/cpp/player/fo3-player-state.h").read_text()
BAR=(BASE/"app/src/main/cpp/player/fo3-barter-state.inc").read_text()

class GameModeCPU(unittest.TestCase):
    def test_immutable_quest_index_does_not_cache_live_running_state(self):
        self.assertIn("q230GameModeQuestIndexReady_",PLAYER)
        self.assertIn("std::sort(q230GameModeQuestIds_.begin()",SCRIPT)
        self.assertIn("for(uint32_t id:q230GameModeQuestIds_)",SCRIPT)
        self.assertIn("saved!=state_.pipboy.quests.end()?saved->second.running",SCRIPT)
        self.assertLess(SCRIPT.index("quests.push_back(id);"),SCRIPT.index("for(const auto id:quests)"))
        self.assertIn('run(q->second.script,"GameMode",id,false)',SCRIPT)
    def test_static_reference_cache_rebuilds_when_scene_changes(self):
        self.assertIn("gFo3VisualPublication.Revision()",LOOP)
        self.assertIn("q230Statics.storage!=gObjects.data()",LOOP)
        self.assertIn("q230Statics.count!=gObjects.size()",LOOP)
        self.assertIn('scriptPlayer.HasReferenceEvent(object.refFormId,"GameMode")',LOOP)
        self.assertIn('scriptPlayer.HasReferenceEvent(actor.source.refFormId,"GameMode")',LOOP)
        self.assertIn("Q230 GAMEMODE CPU: scanUs=",LOOP)
        self.assertIn("PumpWorldAwareGameMode(scriptResidenceElapsed,scriptedResidents",LOOP)
    def test_details_preserve_eye_specific_frustum_and_alpha(self):
        self.assertIn("gQ230DetailedObjectPrevalidated=true;",CORE)
        self.assertIn("gQ230DetailedObjectPrevalidated=previousPrevalidation;",CORE)
        self.assertIn("!Q2015AabbVisible(object)",CORE)
        self.assertIn("for (const GpuObject* alpha : gAlphaFrameOrder)",CORE)
        self.assertIn("Q2017StereoVisible(object)",CORE)
    def test_merchant_restocks_are_not_skipped_when_visits_exist(self):
        self.assertIn("if(!cell||state_.merchantVisits.empty())return;",BAR)
        self.assertIn("RestockMerchant(merchant.first,merchant.second)",BAR)

if __name__=="__main__": unittest.main()
