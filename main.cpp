#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>

struct Target {
    Vector3 position;
    Vector3 size;
    BoundingBox box;
    float health = 100.0f;
    bool isHit = false;
};

// --- ENUM FOR WEAPON TYPES ---
enum WeaponType {
    WEAPON_PISTOL = 0,
    WEAPON_SHOTGUN,
    WEAPON_RIFLE,
    WEAPON_COUNT
};

// --- DATA-DRIVEN WEAPON DATA STRUCT ---
struct Weapon {
    std::string name;
    
    // Weapon Stats
    float damage = 25.0f;
    float fireRate = 0.2f;       // Delay between shots
    float spreadAngle = 0.0f;    // In degrees
    int pelletsPerShot = 1;      // 1 for single bullet, >1 for shotguns
    
    // Recoil Params
    float recoilKick = 0.4f;     // Camera upward kick per shot
    float kickbackZ = 0.2f;      // 3D Model displacement
    
    // Ammo & Reloading
    int maxClip = 12;
    int currentAmmo = 12;
    int totalAmmo = 48;
    float reloadTime = 1.2f;

    // Visuals
    Vector3 modelSize = { 0.1f, 0.15f, 0.5f }; // Gun dimensions
    Color modelColor = DARKGRAY;
};

// --- WEAPON CONTROLLER / INVENTORY STATE ---
struct WeaponController {
    std::vector<Weapon> inventory;
    int activeIndex = 0;
    
    // Active Timers & Recoil State
    float fireTimer = 0.0f;
    bool isReloading = false;
    float reloadTimer = 0.0f;

    float recoilPitch = 0.0f;
    float kickbackZ = 0.0f;

    bool flashActive = false;
    float flashTimer = 0.0f;
    Vector3 muzzlePosition = { 0 };

    Weapon& GetActive() {
        return inventory[activeIndex];
    }

    void SwitchWeapon(int newIndex) {
        if (newIndex < 0 || newIndex >= (int)inventory.size() || newIndex == activeIndex) return;
        
        // Cancel active reload on switch
        isReloading = false;
        reloadTimer = 0.0f;
        fireTimer = 0.1f; // Brief switch delay
        activeIndex = newIndex;
    }
};

// Helper function: Helper to apply random angular spread to a ray direction vector
Vector3 ApplySpread(Vector3 direction, Vector3 up, Vector3 right, float spreadDegrees) {
    if (spreadDegrees <= 0.001f) return direction;

    float radSpread = spreadDegrees * DEG2RAD;
    float randomYaw = GetRandomValue(-1000, 1000) * 0.001f * radSpread;
    float randomPitch = GetRandomValue(-1000, 1000) * 0.001f * radSpread;

    Vector3 offsetDir = Vector3Add(direction, Vector3Scale(right, randomYaw));
    offsetDir = Vector3Add(offsetDir, Vector3Scale(up, randomPitch));
    
    return Vector3Normalize(offsetDir);
}

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "C++ Raylib Shooter - Multi-Weapon Inventory Engine");

    InitAudioDevice();
    Sound fxGunshot = LoadSound("gunshot.wav");
    Sound fxReload  = LoadSound("reload.wav");

    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 10.0f };
    camera.target   = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    // --- SETUP INVENTORY WEAPONS ---
    WeaponController playerWeapons;

    // 1. Pistol (Semi-auto, zero spread, fast reload)
    Weapon pistol;
    pistol.name = "Pistol";
    pistol.damage = 35.0f;
    pistol.fireRate = 0.2f;
    pistol.spreadAngle = 0.2f;
    pistol.pelletsPerShot = 1;
    pistol.recoilKick = 0.3f;
    pistol.kickbackZ = 0.15f;
    pistol.maxClip = 12;
    pistol.currentAmmo = 12;
    pistol.totalAmmo = 48;
    pistol.reloadTime = 1.0f;
    pistol.modelSize = (Vector3){ 0.08f, 0.12f, 0.35f };
    pistol.modelColor = GRAY;

    // 2. Shotgun (High damage, heavy recoil, 8 pellets, wide spread)
    Weapon shotgun;
    shotgun.name = "Shotgun";
    shotgun.damage = 15.0f; // 15 dmg per pellet * 8 pellets = 120 max damage
    shotgun.fireRate = 0.8f;
    shotgun.spreadAngle = 4.5f; // Wide pellet cone
    shotgun.pelletsPerShot = 8;
    shotgun.recoilKick = 1.8f;
    shotgun.kickbackZ = 0.5f;
    shotgun.maxClip = 6;
    shotgun.currentAmmo = 6;
    shotgun.totalAmmo = 24;
    shotgun.reloadTime = 2.2f;
    shotgun.modelSize = (Vector3){ 0.12f, 0.18f, 0.7f };
    shotgun.modelColor = BROWN;

    // 3. Assault Rifle (Fully automatic, high fire rate, progressive recoil & slight spread)
    Weapon rifle;
    rifle.name = "Assault Rifle";
    rifle.damage = 22.0f;
    rifle.fireRate = 0.09f;
    rifle.spreadAngle = 1.2f;
    rifle.pelletsPerShot = 1;
    rifle.recoilKick = 0.25f;
    rifle.kickbackZ = 0.2f;
    rifle.maxClip = 30;
    rifle.currentAmmo = 30;
    rifle.totalAmmo = 120;
    rifle.reloadTime = 1.6f;
    rifle.modelSize = (Vector3){ 0.1f, 0.15f, 0.6f };
    rifle.modelColor = DARKGRAY;

    playerWeapons.inventory = { pistol, shotgun, rifle };

    // Target Setup
    std::vector<Target> targets = {
        { { -4.0f, 1.5f, 0.0f }, { 2.0f, 3.0f, 2.0f }, {}, 100.0f, false },
        { {  0.0f, 1.5f, -3.0f }, { 2.0f, 3.0f, 2.0f }, {}, 100.0f, false },
        { {  4.0f, 1.5f, 0.0f }, { 2.0f, 3.0f, 2.0f }, {}, 100.0f, false }
    };

    for (auto& t : targets) {
        t.box = (BoundingBox){
            (Vector3){ t.position.x - t.size.x/2.0f, t.position.y - t.size.y/2.0f, t.position.z - t.size.z/2.0f },
            (Vector3){ t.position.x + t.size.x/2.0f, t.position.y + t.size.y/2.0f, t.position.z + t.size.z/2.0f }
        };
    }

    // Dynamic Tracer Visual Lines for Shotgun Pellets
    struct Tracer { Vector3 start; Vector3 end; float timer; };
    std::vector<Tracer> activeTracers;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Weapon& activeGun = playerWeapons.GetActive();

        // 1. INPUT: WEAPON SWITCHING (Keys 1, 2, 3 or Scroll Wheel)
        if (IsKeyPressed(KEY_ONE))   playerWeapons.SwitchWeapon(0);
        if (IsKeyPressed(KEY_TWO))   playerWeapons.SwitchWeapon(1);
        if (IsKeyPressed(KEY_THREE)) playerWeapons.SwitchWeapon(2);

        float mouseWheel = GetMouseWheelMove();
        if (mouseWheel > 0) playerWeapons.SwitchWeapon((playerWeapons.activeIndex + 1) % WEAPON_COUNT);
        if (mouseWheel < 0) playerWeapons.SwitchWeapon((playerWeapons.activeIndex - 1 + WEAPON_COUNT) % WEAPON_COUNT);

        // 2. RELOAD & FIRE TIMERS
        if (playerWeapons.fireTimer > 0.0f) playerWeapons.fireTimer -= dt;

        if (playerWeapons.isReloading) {
            playerWeapons.reloadTimer -= dt;
            if (playerWeapons.reloadTimer <= 0.0f) {
                int needed = activeGun.maxClip - activeGun.currentAmmo;
                int ammoToLoad = (activeGun.totalAmmo >= needed) ? needed : activeGun.totalAmmo;
                
                activeGun.currentAmmo += ammoToLoad;
                activeGun.totalAmmo -= ammoToLoad;
                playerWeapons.isReloading = false;
            }
        }

        if (IsKeyPressed(KEY_R) && !playerWeapons.isReloading && activeGun.currentAmmo < activeGun.maxClip && activeGun.totalAmmo > 0) {
            playerWeapons.isReloading = true;
            playerWeapons.reloadTimer = activeGun.reloadTime;
            PlaySound(fxReload);
        }

        // 3. RECOIL DECAY
        playerWeapons.recoilPitch = Lerp(playerWeapons.recoilPitch, 0.0f, 12.0f * dt);
        playerWeapons.kickbackZ = Lerp(playerWeapons.kickbackZ, 0.0f, 15.0f * dt);

        if (playerWeapons.flashActive) {
            playerWeapons.flashTimer -= dt;
            if (playerWeapons.flashTimer <= 0.0f) playerWeapons.flashActive = false;
        }

        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        camera.target = Vector3Add(camera.target, Vector3Scale(camera.up, playerWeapons.recoilPitch * dt));

        // 4. FIRING LOGIC (Supports Single Shots or Automatic Holding based on gun type)
        bool fireInput = (playerWeapons.activeIndex == WEAPON_RIFLE) ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        if (fireInput && playerWeapons.fireTimer <= 0.0f && !playerWeapons.isReloading) {
            if (activeGun.currentAmmo > 0) {
                activeGun.currentAmmo--;
                playerWeapons.fireTimer = activeGun.fireRate;

                SetSoundPitch(fxGunshot, GetRandomValue(90, 110) * 0.01f);
                PlaySound(fxGunshot);

                playerWeapons.flashActive = true;
                playerWeapons.flashTimer = 0.04f;

                playerWeapons.recoilPitch += activeGun.recoilKick;
                playerWeapons.kickbackZ += activeGun.kickbackZ;

                // Base Ray Projection
                Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
                Ray baseRay = GetScreenToWorldRay(screenCenter, camera);

                Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 camRight   = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                Vector3 camUp      = Vector3CrossProduct(camRight, camForward);

                // FIRE PELLETS / BULLETS
                for (int p = 0; p < activeGun.pelletsPerShot; p++) {
                    Vector3 spreadDir = ApplySpread(baseRay.direction, camUp, camRight, activeGun.spreadAngle);
                    Ray pelletRay = { baseRay.position, spreadDir };

                    float closestDistance = 9999.0f;
                    int hitIndex = -1;
                    RayCollision closestHit = { 0 };

                    for (size_t i = 0; i < targets.size(); i++) {
                        RayCollision collision = GetRayCollisionBox(pelletRay, targets[i].box);
                        if (collision.hit && collision.distance < closestDistance) {
                            closestDistance = collision.distance;
                            closestHit = collision;
                            hitIndex = (int)i;
                        }
                    }

                    Vector3 tracerEnd = Vector3Add(pelletRay.position, Vector3Scale(pelletRay.direction, 100.0f));
                    if (hitIndex != -1) {
                        targets[hitIndex].health -= activeGun.damage;
                        if (targets[hitIndex].health <= 0) targets[hitIndex].isHit = true;
                        tracerEnd = closestHit.point;
                    }

                    activeTracers.push_back({ pelletRay.position, tracerEnd, 0.05f });
                }

            } else if (activeGun.totalAmmo > 0) {
                playerWeapons.isReloading = true;
                playerWeapons.reloadTimer = activeGun.reloadTime;
                PlaySound(fxReload);
            }
        }

        // Decay active visual bullet tracers
        for (int i = (int)activeTracers.size() - 1; i >= 0; i--) {
            activeTracers[i].timer -= dt;
            if (activeTracers[i].timer <= 0.0f) {
                activeTracers.erase(activeTracers.begin() + i);
            }
        }

        // --- DRAWING ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);

                for (const auto& t : targets) {
                    Color boxColor = t.isHit ? RED : DARKBLUE;
                    DrawCubeV(t.position, t.size, boxColor);
                    DrawCubeWiresV(t.position, t.size, BLACK);
                }

                // Render active bullet tracers
                for (const auto& tracer : activeTracers) {
                    DrawLine3D(tracer.start, tracer.end, RED);
                }

                // Render Current 3D Gun Model
                Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 camRight   = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                Vector3 camUp      = Vector3CrossProduct(camRight, camForward);

                Vector3 gunPos = camera.position;
                gunPos = Vector3Add(gunPos, Vector3Scale(camRight, 0.4f));
                gunPos = Vector3Add(gunPos, Vector3Scale(camUp, -0.3f));
                gunPos = Vector3Add(gunPos, Vector3Scale(camForward, 0.8f - playerWeapons.kickbackZ));

                playerWeapons.muzzlePosition = Vector3Add(gunPos, Vector3Scale(camForward, activeGun.modelSize.z));

                DrawCube(gunPos, activeGun.modelSize.x, activeGun.modelSize.y, activeGun.modelSize.z, activeGun.modelColor);
                DrawCubeWires(gunPos, activeGun.modelSize.x, activeGun.modelSize.y, activeGun.modelSize.z, BLACK);

                if (playerWeapons.flashActive) {
                    DrawSphere(playerWeapons.muzzlePosition, 0.2f, ORANGE);
                }

            EndMode3D();

            // --- UI & HUD ---
            int crosshairSize = 6;
            DrawLine(screenWidth/2 - crosshairSize, screenHeight/2, screenWidth/2 + crosshairSize, screenHeight/2, GREEN);
            DrawLine(screenWidth/2, screenHeight/2 - crosshairSize, screenWidth/2, screenHeight/2 + crosshairSize, GREEN);

            // Active Weapon & Inventory Display
            DrawText(TextFormat("WEAPON: %s [Slot %i]", activeGun.name.c_str(), playerWeapons.activeIndex + 1), 10, screenHeight - 90, 24, BLACK);
            
            const char* ammoText = TextFormat("AMMO: %i / %i", activeGun.currentAmmo, activeGun.totalAmmo);
            DrawText(ammoText, 10, screenHeight - 50, 30, (activeGun.currentAmmo == 0) ? RED : DARKGRAY);

            if (playerWeapons.isReloading) {
                DrawText("RELOADING...", screenWidth / 2 - 80, screenHeight / 2 + 40, 24, MAROON);
            }

            DrawText("1: Pistol | 2: Shotgun | 3: Rifle | Scroll Wheel: Cycle", 10, 10, 20, DARKGRAY);
            DrawFPS(10, 35);

        EndDrawing();
    }

    UnloadSound(fxGunshot);
    UnloadSound(fxReload);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}