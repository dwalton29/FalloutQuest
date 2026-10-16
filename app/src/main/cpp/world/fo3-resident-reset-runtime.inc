// Explicit context teardown only, never the ordinary scene-transition pump.
void ResetFo3ResidentPreparationLane(){
    // Keep exclusion until all private/displaced owners are gone. Fatal state
    // remains latched until restart; clearing resources cannot repair a world.
    gFo3ResidentTeardownActive=true;
    gFo3ResidentLane.Reset();
    DiscardFo3ResidentGpuStage();
    DiscardFo3ResidentVisualPlan();
    DiscardFo3ResidentLightPlan();
    DiscardFo3ResidentDoorInteractionPlan();
    DiscardFo3ResidentReferencePublicationPlan();
    gFo3ResidentInteractionAdmission.reset();
    gFo3ResidentWorkflow.ResetAfterDrain();
    gFo3ResidentPreparedPacket=nullptr;gFo3ResidentPreparedCollision=0;
    gFo3ResidentTeardownActive=false;
}
