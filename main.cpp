#include "raylib.h"
#include "raymath.h"
#include <vector>

struct Target {
    Vector3 position;
    Vector3 size;
    BoundingBox box;
    bool isHit;
};

struct Weapon {
    int maxClipSize = 10;
    int currentAmmo = 10;
    int totalAmmo = 30;
    float fireRate = 0.15f;
    float fireTimer = 0.0f;
    
    bool isReloading = false;
    float reloadTime = 1.8f;
    float reloadTimer = 0.0f;

    float recoilPitch = 0.0f;
    float kickbackZ = 0.0f;

    // --- MUZZLE FLASH SYSTEM ---
    bool flashActive = false;
    float flashTimer = 0.0f;
    float flashDuration = 0.04f; // Flash lasts for 40 milliseconds
    Vector3 muzzlePosition = { 0 };
};

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "C++ Raylib Shooter - Sound & Muzzle Flash");
    
    // 1. INITIALIZE AUDIO DEVICE
    InitAudioDevice();

    // 2. LOAD SOUND EFFECTS
    Sound fxGunshot = LoadSound("gunshot.wav");
    Sound fxReload  = LoadSound("reload.wav");

    // Adjust sound volume
    SetSoundVolume(fxGunshot, 0.8f);
    SetSoundVolume(fxReload, 0.6f);

    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 10.0f };
    camera.target   = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    Weapon rifle;

    std::vector<Target> targets = {
        { { -4.0f, 1.5f, 0.0f }, { 2.0f, 3.0f, 2.0f }, {}, false },
        { {  0.0f, 1.5f, -3.0f }, { 2.0f, 3.0f, 2.0f }, {}, false },
        { {  4.0f, 1.5f, 0.0f }, { 2.0f, 3.0f, 2.0f }, {}, false }
    };

    for (auto& t : targets) {
        t.box = (BoundingBox){
            (Vector3){ t.position.x - t.size.x/2.0f, t.position.y - t.size.y/2.0f, t.position.z - t.size.z/2.0f },
            (Vector3){ t.position.x + t.size.x/2.0f, t.position.y + t.size.y/2.0f, t.position.z + t.size.z/2.0f }
        };
    }

    RayCollision closestHit = { 0 };
    bool rayActive = false;
    Vector3 rayStart = { 0 };
    Vector3 rayEnd = { 0 };
    float rayTimer = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (rifle.fireTimer > 0.0f) rifle.fireTimer -= dt;

        // Reload Logic & Sound
        if (rifle.isReloading) {
            rifle.reloadTimer -= dt;
            if (rifle.reloadTimer <= 0.0f) {
                int neededAmmo = rifle.maxClipSize - rifle.currentAmmo;
                int ammoToLoad = (rifle.totalAmmo >= neededAmmo) ? neededAmmo : rifle.totalAmmo;
                
                rifle.currentAmmo += ammoToLoad;
                rifle.totalAmmo -= ammoToLoad;
                rifle.isReloading = false;
            }
        }

        // Trigger manual reload
        if (IsKeyPressed(KEY_R) && !rifle.isReloading && rifle.currentAmmo < rifle.maxClipSize && rifle.totalAmmo > 0) {
            rifle.isReloading = true;
            rifle.reloadTimer = rifle.reloadTime;
            
            // Play reload audio
            PlaySound(fxReload);
        }

        // Recoil decay
        rifle.recoilPitch = Lerp(rifle.recoilPitch, 0.0f, 12.0f * dt);
        rifle.kickbackZ = Lerp(rifle.kickbackZ, 0.0f, 15.0f * dt);

        // Manage Muzzle Flash Timer
        if (rifle.flashActive) {
            rifle.flashTimer -= dt;
            if (rifle.flashTimer <= 0.0f) {
                rifle.flashActive = false;
            }
        }

        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        camera.target = Vector3Add(camera.target, Vector3Scale(camera.up, rifle.recoilPitch * dt));

        // FIRING LOGIC
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && rifle.fireTimer <= 0.0f && !rifle.isReloading) {
            if (rifle.currentAmmo > 0) {
                rifle.currentAmmo--;
                rifle.fireTimer = rifle.fireRate;

                // --- 1. PLAY GUNSHOT AUDIO ---
                // Randomize pitch slightly so every shot sounds natural and non-repetitive
                SetSoundPitch(fxGunshot, GetRandomValue(95, 105) * 0.01f);
                PlaySound(fxGunshot);

                // --- 2. TRIGGER MUZZLE FLASH ---
                rifle.flashActive = true;
                rifle.flashTimer = rifle.flashDuration;

                // Recoil
                rifle.recoilPitch += GetRandomValue(3, 5) * 0.1f;
                rifle.kickbackZ += 0.25f;

                // Hitscan Raycast
                Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
                Ray ray = GetScreenToWorldRay(screenCenter, camera);

                float closestDistance = 9999.0f;
                int hitIndex = -1;

                for (size_t i = 0; i < targets.size(); i++) {
                    RayCollision collision = GetRayCollisionBox(ray, targets[i].box);
                    if (collision.hit && collision.distance < closestDistance) {
                        closestDistance = collision.distance;
                        closestHit = collision;
                        hitIndex = (int)i;
                    }
                }

                rayStart = ray.position;
                if (hitIndex != -1) {
                    targets[hitIndex].isHit = true;
                    rayEnd = closestHit.point;
                } else {
                    rayEnd = Vector3Add(ray.position, Vector3Scale(ray.direction, 100.0f));
                }

                rayActive = true;
                rayTimer = 0.05f;
            } else if (rifle.totalAmmo > 0) {
                rifle.isReloading = true;
                rifle.reloadTimer = rifle.reloadTime;
                PlaySound(fxReload);
            }
        }

        if (rayActive) {
            rayTimer -= dt;
            if (rayTimer <= 0.0f) rayActive = false;
        }

        // --- DRAWING ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);

                for (const auto& t : targets) {
                    Color boxColor = t.isHit ? RED : DARKBLUE;
                    
                    // Illuminate target boxes dynamically if flash is active
                    if (rifle.flashActive) {
                        boxColor = YELLOW; // Simple ambient burst reaction
                    }

                    DrawCubeV(t.position, t.size, boxColor);
                    DrawCubeWiresV(t.position, t.size, BLACK);
                }

                if (rayActive) {
                    DrawLine3D(rayStart, rayEnd, RED);
                    if (closestHit.hit) DrawSphere(closestHit.point, 0.15f, YELLOW);
                }

                // --- GUN & MUZZLE POSITIONS ---
                Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 camRight   = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                Vector3 camUp      = Vector3CrossProduct(camRight, camForward);

                Vector3 gunPos = camera.position;
                gunPos = Vector3Add(gunPos, Vector3Scale(camRight, 0.4f));
                gunPos = Vector3Add(gunPos, Vector3Scale(camUp, -0.3f));
                gunPos = Vector3Add(gunPos, Vector3Scale(camForward, 0.8f - rifle.kickbackZ));

                // Muzzle tip position (placed at tip of gun barrel)
                rifle.muzzlePosition = Vector3Add(gunPos, Vector3Scale(camForward, 0.4f));

                // Draw Gun Mesh
                DrawCube(gunPos, 0.1f, 0.15f, 0.6f, DARKGRAY);
                DrawCubeWires(gunPos, 0.1f, 0.15f, 0.6f, BLACK);

                // --- DRAW MUZZLE FLASH GEOMETRY ---
                if (rifle.flashActive) {
                    // Core flash sphere
                    DrawSphere(rifle.muzzlePosition, 0.18f, ORANGE);
                    
                    // Outer flash glow
                    DrawSphere(rifle.muzzlePosition, 0.35f, ColorAlpha(YELLOW, 0.6f));
                }

            EndMode3D();

            // HUD
            int crosshairSize = 6;
            DrawLine(screenWidth/2 - crosshairSize, screenHeight/2, screenWidth/2 + crosshairSize, screenHeight/2, GREEN);
            DrawLine(screenWidth/2, screenHeight/2 - crosshairSize, screenWidth/2, screenHeight/2 + crosshairSize, GREEN);

            const char* ammoText = TextFormat("AMMO: %i / %i", rifle.currentAmmo, rifle.totalAmmo);
            DrawText(ammoText, screenWidth - 220, screenHeight - 60, 30, (rifle.currentAmmo == 0) ? RED : BLACK);

            if (rifle.isReloading) {
                DrawText("RELOADING...", screenWidth / 2 - 80, screenHeight / 2 + 40, 24, MAROON);
            }

            DrawText("Left Click: Fire | R: Reload", 10, 10, 20, DARKGRAY);
            DrawFPS(10, 40);

        EndDrawing();
    }

    // UNLOAD AUDIO RESOURCES & CLOSE DEVICE
    UnloadSound(fxGunshot);
    UnloadSound(fxReload);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}