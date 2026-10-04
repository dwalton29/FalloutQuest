"""Ensure the live prompt and loot renderers share Fallout glyph geometry."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
CPP=ROOT/'app/src/main/cpp'
class FontConsumers(unittest.TestCase):
    def test_one_live_layout_and_cached_geometry(self):
        ui=CPP/'ui/interaction'
        renderer=(ui/'fo3-interaction-hud-renderer.h').read_text()
        loot=(ui/'fo3-loot-panel.h').read_text()
        loop=(CPP/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('fo3font::AppendText(',renderer)
        self.assertIn('fo3font::AppendText(',loot)
        self.assertIn('fo3font::FitText(',loot)
        self.assertIn('fo3font::MeasureText(',renderer)
        self.assertIn('fo3hudrenderer::EnsureResources()',loot)
        self.assertIn('fo3hudrenderer::GlStateGuard guard',loot)
        self.assertIn('GlStateGuard guard;',renderer)
        self.assertIn('SameBuiltPrompt(s, promptChars, length)',renderer)
        self.assertIn('if (key != r.key)',loot)
        self.assertIn('RenderFo3InteractionHud(mvp.m, doorPromptQ1850_.data())',loop)
        self.assertIn('fo3lootui::Render(panelMvp.m,loot)',loop)
        self.assertNotIn('fo3huddynamic::',loot+loop)
        for path in ui.glob('*.h'):
            source=path.read_text()
            for obsolete in ('.xOffset','.yOffset','.advance','lineHeight','maxDrawableHeight','referenceYOffset'):
                self.assertNotIn(obsolete,source,str(path))
        self.assertIn('fo3hudrenderer::Render(mvp,prompt)',(ui/'fo3-interaction-hud-layout.h').read_text())
    def test_diagnostics_do_not_mutate_or_run_per_eye(self):
        ui=CPP/'ui/interaction'
        diagnostics=(ui/'fo3-font-diagnostics.h').read_text()
        self.assertIn('debug.falloutquest.font',diagnostics)
        self.assertIn('>=96',diagnostics)
        renderer=(ui/'fo3-interaction-hud-renderer.h').read_text()
        geometry=renderer.split('inline bool BuildPromptGeometry',1)[1].split('inline void Render(',1)[0]
        self.assertLess(geometry.index('SameBuiltPrompt('),geometry.index('LogTextDiagnostics('))
        self.assertIn('byte!=0x20&&byte!=0x2d&&byte!=0x2e',renderer)
        self.assertNotIn('Super-Duper',renderer+diagnostics)
        loop=(CPP/'core/fo3-runtime-loop.inc').read_text()
        eye=loop.split('bool RenderEye(',1)[1]
        self.assertNotIn('fo3fontdebug::LogBytes(',eye)
if __name__=='__main__':unittest.main()
