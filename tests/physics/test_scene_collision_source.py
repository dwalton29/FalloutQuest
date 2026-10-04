from pathlib import Path
s=Path('app/src/main/cpp/core/fo3-runtime.cpp').read_text()
begin=s[s.index('bool BeginFo3SceneLoad('):s.index('bool ProcessQ74TransitionRequest(')]
assert begin.index('SelectFo3AuthoredCollisionPlacements(result.placements)') < begin.index('if (Q74ShouldSkipPlacement(placement))') < begin.index('if (!BuildCpuObjects(placement, parts))')
transition=s[s.index('bool ProcessQ74TransitionRequest('):]
assert 'if (request.worldspaceFormId != 0u) ConfigureFo3CollisionPolicyQ710' not in transition
assert transition.index('ResetFo3CollisionSceneCache()') < transition.index('collisionPreparation.Start(')
assert transition.index('PublishFo3CollisionSnapshotQ1930') < transition.index('gObjects = std::move(replacement)') < transition.index('CompleteFo3CellTransitionQ74(')
assert transition.index('gSceneLoad.collisionComplete = true') < transition.index('PublishFo3CollisionSnapshotQ1930')
print('Scene collision independence and publication ordering passed')
