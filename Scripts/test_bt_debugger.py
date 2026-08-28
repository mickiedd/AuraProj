"""Focused parser/model contracts for Scripts/bt_debugger.py."""

from __future__ import annotations

from pathlib import Path
import os
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Scripts"))

from bt_debugger import (  # noqa: E402
    BTLogParser,
    LogModel,
    UNATTRIBUTED_KEY,
    discover_log_candidates,
)


class BTLogParserTests(unittest.TestCase):
    def test_correlates_contextual_civilian_messages_to_tree(self) -> None:
        parser = BTLogParser()
        infos = {}
        started = parser.parse(
            "LogAura: Display: [CivilianAI] BT started pawn=AuraCivilian_0 tree=BT_Civilian_Runtime.",
            1,
            infos,
        )
        activity = parser.parse(
            "LogAura: [CivilianAI][BT] AuraCivilian_0 destination=V(X=1, Y=2, Z=3).",
            2,
            infos,
        )

        self.assertIn("BT_Civilian_Runtime", started.keys)
        self.assertEqual(activity.keys, frozenset({"BT_Civilian_Runtime"}))
        self.assertEqual(infos["BT_Civilian_Runtime"].line_count, 2)

    def test_keeps_task_lines_visible_when_tree_identity_is_missing(self) -> None:
        parser = BTLogParser()
        infos = {}
        entry = parser.parse(
            "LogAura: [BTTask_MoveToTarget] Pawn=Enemy_0 — MoveRequest FAILED.",
            7,
            infos,
        )

        self.assertIn("source:BTTask_MoveToTarget", entry.keys)
        self.assertIn(UNATTRIBUTED_KEY, entry.keys)
        self.assertEqual(infos["source:BTTask_MoveToTarget"].kind, "source")
        self.assertEqual(infos[UNATTRIBUTED_KEY].kind, "unattributed")

    def test_ignores_behavioru_runtime_object_name(self) -> None:
        parser = BTLogParser()
        self.assertIsNone(parser.normalize_tree_name("BehaviorUBehaviorTree_3"))
        self.assertEqual(parser.normalize_tree_name("C:/Content/BT_TestEnemy.xml"), "BT_TestEnemy")


class LogModelTests(unittest.TestCase):
    def test_load_and_poll_tracks_append_and_filters_enabled_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "server.log"
            path.write_text(
                "[CivilianAI] BT started pawn=Civilian_0 tree=BT_Civilian_Runtime\n"
                "[CivilianAI][BT] Civilian_0 destination=V(X=1, Y=2, Z=3)\n"
                "[BTTask_MoveToTarget] Pawn=Enemy_0 — MoveRequest FAILED\n",
                encoding="utf-8",
            )

            model = LogModel(max_lines=20)
            model.load(path)
            model.infos["BT_Civilian_Runtime"].enabled = False
            visible = model.visible_entries(None, True, "")
            self.assertEqual(len(visible), 1)
            self.assertIn("BTTask_MoveToTarget", visible[0].text)

            with path.open("a", encoding="utf-8") as handle:
                handle.write("[CivilianAI][BT] Civilian_0 destination=V(X=4, Y=5, Z=6)\n")
            self.assertTrue(model.poll())
            self.assertEqual(len(model.entries), 4)
            selected = model.visible_entries("BT_Civilian_Runtime", False, "")
            self.assertEqual(selected, [])

    def test_discover_log_candidates_prefers_server_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            log_dir = root / "Saved" / "Logs"
            log_dir.mkdir(parents=True)
            (log_dir / "Aura.log").write_text("", encoding="utf-8")
            (log_dir / "ExampleServer.log").write_text("", encoding="utf-8")
            candidates = discover_log_candidates(root)
            self.assertEqual([path.name for path in candidates], ["ExampleServer.log"])

    def test_poll_rescans_replaced_file_even_when_new_file_is_larger(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "server.log"
            path.write_text(
                "[CivilianAI] BT started pawn=Old tree=BT_Old\n",
                encoding="utf-8",
            )
            model = LogModel(max_lines=20)
            model.load(path)

            replacement = Path(temp_dir) / "replacement.log"
            replacement.write_text(
                "[CivilianAI] BT started pawn=New tree=BT_New\n"
                "[CivilianAI][BT] New destination=V(X=4, Y=5, Z=6)\n",
                encoding="utf-8",
            )
            os.replace(replacement, path)

            self.assertTrue(model.poll())
            self.assertEqual([entry.text for entry in model.entries], [
                "[CivilianAI] BT started pawn=New tree=BT_New",
                "[CivilianAI][BT] New destination=V(X=4, Y=5, Z=6)",
            ])


if __name__ == "__main__":
    unittest.main()
