#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>

// --- ENEMY AI STRUCT ---
enum AIState { AI_CHASE, AI_ATTACK, AI_DEAD };

struct Enemy {
    Vector3 position;
    Vector3 size;
    BoundingBox box;
    
    float health = 100.0f;
    float maxHealth = 100.0f;
    float speed = 2.5f;
    
    // Attack parameters
    float attackRange = 2.2f;
    float attackDamage = 15.0f;
    float attackCooldown = 1.0f;
    float attackTimer = 0.0f;
    
    AIState state = AI_CHASE;
    Color color = RED;

    void Update(float dt, Vector3 playerPos, float& playerHealth) {
        if (state == AI_DEAD) return;

        // Calculate distance to player on XZ plane (ignore Y height difference)
        Vector3 toPlayer = Vector3Subtract(playerPos, position);
        toPlayer.y = 0.0f; // Keep movement grounded on flat terrain
        float distance = Vector3Length(toPlayer);

        // Update Bounding Box position
        box = (BoundingBox){
            (Vector3){ position.x - size.x/2.0f, position.y - size.y/2.0f, position.z - size.z/2.0f },
            (Vector3){ position.x + size.x/2.0f, position.y + size.y/2.0f, position.z + size.z/2.0f }
        };

        // --- STATE MACHINE ---
        if (distance <= attackRange) {
            state = AI_ATTACK;
        } else {
            state = AI_CHASE;
        }

        // --- BEHAVIOR EXECUTION ---
        if (state == AI_CHASE) {
            // Move toward player position
            Vector3 moveDir = Vector3Normalize(toPlayer);
            position = Vector3Add(position, Vector3Scale(moveDir, speed * dt));
            color = RED;
        } 
        else if (state == AI_ATTACK) {
            color = MAROON; // Flash darker when in attack range
            attackTimer -= dt;
            
            if (attackTimer <= 0.0f) {
                // Attack the player!
                playerHealth -= attackDamage;
                if (playerHealth < 0.0f) playerHealth = 0.0f;
                attackTimer = attackCooldown; // Reset attack timer
            }
        }
    }

    void TakeDamage(float dmg) {
        if (state == AI_DEAD) return;
        health -= dmg;
        if (health <= 0.0f) {
            health = 0.0f;
            state = AI_DEAD;
        }
    }
};

// --- WEAPON STRUCT ---
struct Weapon {
    std::string name;
    float damage = 25.0f;
    float fireRate = 0.2f;
    float spreadAngle = 0.0f;
    int pelletsPerShot = 1;
    float recoilKick = 0.4f;
    float kickbackZ = 0.2f;
    int maxClip = 12;
    int currentAmmo = 12;
    int totalAmmo = 48;
    float reloadTime = 1.2f;
    Vector3 modelSize = { 0.1f, 0.15f, 0.5f };
    Color modelColor = DARKGRAY;
};

struct WeaponController {
    std::vector<Weapon> inventory;
    int activeIndex = 0;
    float fireTimer = 0.0f;
    bool isReloading = false;
    float reloadTimer = 0.0f;
    float recoilPitch = 0.0f;
    float kickbackZ = 0.0f;
    bool flashActive = false;
    float flashTimer = 0.0f;
    Vector3 muzzlePosition = { 0 };

    Weapon& GetActive() { return inventory[activeIndex]; }

    void SwitchWeapon(int newIndex) {
        if (newIndex < 0 || newIndex >= (int)inventory.size() || newIndex == activeIndex) return;
        isReloading = false;
        reloadTimer = 0.0f;
        fireTimer = 0.1f;
        activeIndex = newIndex;
    }
};

Vector3 ApplySpread(Vector3 direction, Vector3 up, Vector3 right, float spreadDegrees) {
    if (spreadDegrees <= 0.001f) return direction;
    float radSpread = spreadDegrees * DEG2RAD;
    float randomYaw = GetRandomValue(-1000, 1000) * 0.001f * radSpread;
    float randomPitch = GetRandomValue(-1000, 1000) * 0.001f * radSpread;
    Vector3 offsetDir = Vector3Add(direction, Vector3Scale(right, randomYaw));
    offsetDir = Vector3Add(offsetDir, Vector3Scale(up, randomPitch));
    return Vector3Normalize(offsetDir);
}

// Function to spawn a random enemy outside player view
Enemy SpawnRandomEnemy() {
    Enemy e;
    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float dist = (float)GetRandomValue(15, 25);
    
    e.position = (Vector3){ cosf(angle) * dist, 1.25f, sinf(angle) * dist };
    e.size = (Vector3){ 1.5f, 2.5f, 1.5f };
    e.health = 100.0f;
    e.maxHealth = 100.0f;
    e.speed = (float)GetRandomValue(20, 35) * 0.1f;
    return e;
}

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "C++ Raylib Shooter - Enemy AI System");

    InitAudioDevice();
    Sound fxGunshot = LoadSound("gunshot.wav");
    Sound fxReload  = LoadSound("reload.wav");

    SetTargetFPS(60);

    // Player State
    float playerHealth = 100.0f;
    float playerMaxHealth = 100.0f;
    int score = 0;

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 10.0f };
    camera.target   = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    // Weapon Inventory Setup
    WeaponController playerWeapons;

    Weapon pistol;
    pistol.name = "Pistol"; pistol.damage = 40.0f; pistol.fireRate = 0.2f; pistol.spreadAngle = 0.1f;
    pistol.pelletsPerShot = 1; pistol.recoilKick = 0.3f; pistol.kickbackZ = 0.15f;
    pistol.maxClip = 12; pistol.currentAmmo = 12; pistol.totalAmmo = 48; pistol.reloadTime = 1.0f;
    pistol.modelSize = { 0.08f, 0.12f, 0.35f }; pistol.modelColor = GRAY;

    Weapon shotgun;
    shotgun.name = "Shotgun"; shotgun.damage = 18.0f; shotgun.fireRate = 0.8f; shotgun.spreadAngle = 4.5f;
    shotgun.pelletsPerShot = 8; shotgun.recoilKick = 1.8f; shotgun.kickbackZ = 0.5f;
    shotgun.maxClip = 6; shotgun.currentAmmo = 6; shotgun.totalAmmo = 24; shotgun.reloadTime = 2.2f;
    shotgun.modelSize = { 0.12f, 0.18f, 0.7f }; shotgun.modelColor = BROWN;

    Weapon rifle;
    rifle.name = "Assault Rifle"; rifle.damage = 25.0f; rifle.fireRate = 0.09f; rifle.spreadAngle = 1.2f;
    rifle.pelletsPerShot = 1; rifle.recoilKick = 0.25f; rifle.kickbackZ = 0.2f;
    rifle.maxClip = 30; rifle.currentAmmo = 30; rifle.totalAmmo = 120; rifle.reloadTime = 1.6f;
    rifle.modelSize = { 0.1f, 0.15f, 0.6f }; rifle.modelColor = DARKGRAY;

    playerWeapons.inventory = { pistol, shotgun, rifle };

    // Initial Enemy Spawns
    std::vector<Enemy> enemies;
    for (int i = 0; i < 4; i++) enemies.push_back(SpawnRandomEnemy());

    struct Tracer { Vector3 start; Vector3 end; float timer; };
    std::vector<Tracer> activeTracers;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Weapon& activeGun = playerWeapons.GetActive();

        // Game Over Reset
        if (playerHealth <= 0.0f) {
            if (IsKeyPressed(KEY_R)) {
                playerHealth = 100.0f;
                score = 0;
                enemies.clear();
                for (int i = 0; i < 4; i++) enemies.push_back(SpawnRandomEnemy());
            }
        } else {
            // --- 1. UPDATE ENEMY AI ---
            for (size_t i = 0; i < enemies.size(); i++) {
                enemies[i].Update(dt, camera.position, playerHealth);

                // Respawning Dead Enemies
                if (enemies[i].state == AI_DEAD) {
                    score += 100;
                    enemies[i] = SpawnRandomEnemy(); // Replace with new enemy
                }
            }

            // --- 2. INPUT & WEAPON CONTROLS ---
            if (IsKeyPressed(KEY_ONE))   playerWeapons.SwitchWeapon(0);
            if (IsKeyPressed(KEY_TWO))   playerWeapons.SwitchWeapon(1);
            if (IsKeyPressed(KEY_THREE)) playerWeapons.SwitchWeapon(2);

            float mouseWheel = GetMouseWheelMove();
            if (mouseWheel > 0) playerWeapons.SwitchWeapon((playerWeapons.activeIndex + 1) % 3);
            if (mouseWheel < 0) playerWeapons.SwitchWeapon((playerWeapons.activeIndex - 1 + 3) % 3);

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

            // Recoil
            playerWeapons.recoilPitch = Lerp(playerWeapons.recoilPitch, 0.0f, 12.0f * dt);
            playerWeapons.kickbackZ = Lerp(playerWeapons.kickbackZ, 0.0f, 15.0f * dt);

            if (playerWeapons.flashActive) {
                playerWeapons.flashTimer -= dt;
                if (playerWeapons.flashTimer <= 0.0f) playerWeapons.flashActive = false;
            }

            UpdateCamera(&camera, CAMERA_FIRST_PERSON);
            camera.target = Vector3Add(camera.target, Vector3Scale(camera.up, playerWeapons.recoilPitch * dt));

            // --- 3. FIRING LOGIC & RAYCAST DAMAGE ---
            bool fireInput = (playerWeapons.activeIndex == 2) ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

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

                    Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
                    Ray baseRay = GetScreenToWorldRay(screenCenter, camera);

                    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                    Vector3 camRight   = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
                    Vector3 camUp      = Vector3CrossProduct(camRight, camForward);

                    for (int p = 0; p < activeGun.pelletsPerShot; p++) {
                        Vector3 spreadDir = ApplySpread(baseRay.direction, camUp, camRight, activeGun.spreadAngle);
                        Ray pelletRay = { baseRay.position, spreadDir };

                        float closestDistance = 9999.0f;
                        int hitIndex = -1;
                        RayCollision closestHit = { 0 };

                        for (size_t i = 0; i < enemies.size(); i++) {
                            if (enemies[i].state == AI_DEAD) continue;
                            RayCollision collision = GetRayCollisionBox(pelletRay, enemies[i].box);
                            if (collision.hit && collision.distance < closestDistance) {
                                closestDistance = collision.distance;
                                closestHit = collision;
                                hitIndex = (int)i;
                            }
                        }

                        Vector3 tracerEnd = Vector3Add(pelletRay.position, Vector3Scale(pelletRay.direction, 100.0f));
                        if (hitIndex != -1) {
                            enemies[hitIndex].TakeDamage(activeGun.damage);
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
        }

        // Decay bullet tracers
        for (int i = (int)activeTracers.size() - 1; i >= 0; i--) {
            activeTracers[i].timer -= dt;
            if (activeTracers[i].timer <= 0.0f) activeTracers.erase(activeTracers.begin() + i);
        }

        // --- DRAWING ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(30, 1.0f);

                // Render AI Enemies + Health Bars
                for (const auto& e : enemies) {
                    if (e.state == AI_DEAD) continue;

                    DrawCubeV(e.position, e.size, e.color);
                    DrawCubeWiresV(e.position, e.size, BLACK);

                    // 3D Billboard Healthbar above enemy head
                    Vector3 healthBarPos = { e.position.x, e.position.y + e.size.y/2.0f + 0.4f, e.position.z };
                    float healthPct = e.health / e.maxHealth;
                    DrawBillboardRec(camera, (Texture2D){ 0 }, (Rectangle){ 0, 0, 1, 1 }, healthBarPos, (Vector2){ 1.2f, 0.15f }, WHITE);
                    DrawCube(healthBarPos, 1.2f * healthPct, 0.1f, 0.05f, GREEN);
                }

                for (const auto& tracer : activeTracers) {
                    DrawLine3D(tracer.start, tracer.end, RED);
                }

                // Gun Model
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

                if (playerWeapons.flashActive) DrawSphere(playerWeapons.muzzlePosition, 0.2f, ORANGE);

            EndMode3D();

            // --- HUD & HEALTH UI ---
            int crosshairSize = 6;
            DrawLine(screenWidth/2 - crosshairSize, screenHeight/2, screenWidth/2 + crosshairSize, screenHeight/2, GREEN);
            DrawLine(screenWidth/2, screenHeight/2 - crosshairSize, screenWidth/2, screenHeight/2 + crosshairSize, GREEN);

            // Player Health Bar
            DrawRectangle(20, screenHeight - 40, 200, 20, DARKGRAY);
            DrawRectangle(20, screenHeight - 40, (int)(200 * (playerHealth / playerMaxHealth)), 20, RED);
            DrawRectangleLines(20, screenHeight - 40, 200, 20, BLACK);
            DrawText(TextFormat("HEALTH: %i HP", (int)playerHealth), 25, screenHeight - 38, 16, WHITE);

            // Ammo & Score UI
            DrawText(TextFormat("SCORE: %i", score), screenWidth - 180, 20, 24, GOLD);
            DrawText(TextFormat("WEAPON: %s", activeGun.name.c_str()), 20, 20, 22, BLACK);
            DrawText(TextFormat("AMMO: %i / %i", activeGun.currentAmmo, activeGun.totalAmmo), 20, 50, 22, (activeGun.currentAmmo == 0) ? RED : DARKGRAY);

            if (playerWeapons.isReloading) {
                DrawText("RELOADING...", screenWidth / 2 - 80, screenHeight / 2 + 40, 24, MAROON);
            }

            if (playerHealth <= 0.0f) {
                DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
                DrawText("YOU DIED", screenWidth/2 - 120, screenHeight/2 - 40, 50, RED);
                DrawText("Press 'R' to Restart", screenWidth/2 - 110, screenHeight/2 + 20, 20, RAYWHITE);
            }

        EndDrawing();
    }

    UnloadSound(fxGunshot);
    UnloadSound(fxReload);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}