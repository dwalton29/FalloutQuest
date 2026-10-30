// Exercise the real scene controller plus gravity, including authored NIF data.
static void StairQuad(const Vec3& a,const Vec3& b,const Vec3& c,const Vec3& d){
    CollisionTriangle t;assert(BuildTriangle(a,b,c,t));gWorldTriangles.push_back(t);
    assert(BuildTriangle(a,c,d,t));gWorldTriangles.push_back(t);
}
static void StairStart(){
    gPlayerCollisionReady=true;gSafeSpawnResolved=true;gCollisionFloorY=0;
    gExteriorAllBhksQ78A=false;PLAYER_HEIGHT=1.7f;terrainAvailable=false;
    gFallGrounded=true;gFallVelocity=0;
    InvalidateDerivedCollisionCachesQ17();AdvanceFo3CollisionPublicationRevision();Q225RebuildDynamicGrid();
}
static void WalkStair(float& x,float& z,float& y,float dz,int frames){
    for(int n=0;n<frames;++n){float nx,ny,nz;
        assert(ResolveFo3PlayerMotionWithGravity(x,z,x,z+dz,y,1.f/72,&nx,&nz,&ny));
        assert(std::isfinite(nx)&&std::isfinite(ny)&&std::isfinite(nz));
        x=nx;y=ny;z=nz;
    }
}
static void InteriorStepRegression(){
    for(int variant=0;variant<4;++variant){
        ResetFo3CollisionSceneCache();gWorldTriangles.clear();gSurfaceSourcesQ722.clear();
        StairQuad({-1,0,1},{1,0,1},{1,0,-4},{-1,0,-4});
        const float rise=variant==1?.65f:.4f;
        for(int n=0;n<8;++n){const float front=-n*.45f,back=front-.45f,top=(n+1)*rise;
            StairQuad({-1,top,front},{1,top,front},{1,top,back},{-1,top,back});
            StairQuad({-1,top-rise,front},{1,top-rise,front},{1,top,front},{-1,top,front});
        }
        if(variant>=2){const float ceiling=variant==3?1.f:1.9f;StairQuad({-1,ceiling,1},{1,ceiling,1},{1,ceiling,-4},{-1,ceiling,-4});}
        StairStart();if(variant==3)PLAYER_HEIGHT=.7f;float x=0,z=.3f,y=0;
        // Ground contact stays stable under submillimetre gravity integration.
        for(int n=0;n<120;++n){WalkStair(x,z,y,0,1);assert(gFallGrounded&&std::abs(y)<.005f);}
        WalkStair(x,z,y,-.025f,135);
        if(variant==0){assert(z< -2.9f&&y>2.7f);WalkStair(x,z,y,.025f,135);WalkStair(x,z,y,0,90);assert(z>.2f&&std::abs(y)<.04f);}
        else assert(z>.15f&&y<.05f); // Tall risers and ceiling still block.
    }
    PLAYER_HEIGHT=1.7f;
    std::cout<<"Interior stairs: ascent/descent, stable support, tall risers and ceiling clearance passed\n";
}
static void OriginalInteriorStairs(const char* left,const char* right){
    for(int side=0;side<2;++side){
        const char* path=side?right:left;std::ifstream f(path,std::ios::binary);
        const std::string model=side?"original-stairs-right.nif":"original-stairs-left.nif";
        assets[model]={std::istreambuf_iterator<char>(f),{}};assert(!assets[model].empty());
        publish(prepare({placement(980,model)},false,501+side));assert(gWorldTriangles.size()>100);
        StairStart();float x=side?-.5f:.5f,z=.4f,y=-.971562f;
        WalkStair(x,z,y,-.025f,180);assert(z< -3.9f&&y>2.15f&&y<2.5f);
        // Clear the last tread by the capsule radius before checking floor height.
        WalkStair(x,z,y,.025f,190);WalkStair(x,z,y,0,90);assert(z>.55f&&y<-.85f&&y> -1.1f);
        std::cout<<"Original "<<(side?"right":"left")<<" shack stairs: production controller/gravity ascent and descent passed\n";
    }
}
