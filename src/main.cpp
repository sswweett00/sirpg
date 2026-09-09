#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <entt/entt.hpp>
#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <memory>
#include <iostream>

#include "Core/Logger.hpp"
#include "Core/ArenaAllocator.hpp"
#include "Core/ObjectPool.hpp"
#include "Core/TimeStep.hpp"
#include "Core/Profiler.hpp"
#include "Components/Components.hpp"
#include "Engine/TextureAtlas.hpp"
#include "Engine/BatchedRenderer.hpp"
#include "Systems/Systems.hpp"

using namespace sirpg::core;
using namespace sirpg::components;
using namespace sirpg::engine;
using namespace sirpg::systems;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::init();
    LOG_INFO("Starting SIRPG Production-Ready C++23 Engine & Side-Scroller RPG...");

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        LOG_CRITICAL("Failed to initialize SDL3: {}", SDL_GetError());
        return 1;
    }

    const int windowWidth = 1280;
    const int windowHeight = 720;

    SDL_Window* window = SDL_CreateWindow(
        "SIRPG 2D Engine - Production C++23 / DOD / EnTT / Box2D v3",
        windowWidth, windowHeight,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        LOG_CRITICAL("Failed to create SDL3 window: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        LOG_CRITICAL("Failed to create SDL3 renderer: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);

    // Arena Allocator for general engine pre-allocated memory (16MB)
    ArenaAllocator engineArena(16 * 1024 * 1024);
    LOG_INFO("Engine Arena Allocator initialized with {} MB", engineArena.getCapacity() / (1024 * 1024));

    // Object Pools for Zero Runtime Memory Allocations (Floating Texts & Particles)
    ObjectPool<FloatingText, 128> floatingTextPool;
    ObjectPool<Particle, 256> particlePool;

    // EnTT Registry
    entt::registry registry;

    // Box2D v3 Physics World
    b2WorldId physicsWorld = PhysicsSystem::createWorld();

    // Procedural Texture Atlas
    TextureAtlas textureAtlas;
    if (!textureAtlas.generate(renderer, 512, 512)) {
        LOG_CRITICAL("Failed to generate texture atlas");
        return 1;
    }

    BatchedRenderer batchedRenderer;
    Camera camera;
    camera.viewportSize = glm::vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));

    // =========================================================================
    // MAP & ENTITY INITIALIZATION (100x20 Tilemap World)
    // =========================================================================
    constexpr int mapWidth = 100;
    constexpr int mapHeight = 20;
    constexpr float tileSize = 32.0f;

    // 1. Create Parallax Background Entities
    auto bgEntity1 = registry.create();
    registry.emplace<ParallaxComponent>(bgEntity1, ParallaxComponent{.scrollFactor = 0.2f, .baseOffset = glm::vec2(0.0f, 0.0f)});
    registry.emplace<SpriteComponent>(bgEntity1, SpriteComponent{
        .srcRect = SDL_FRect{0.0f, 256.0f, 256.0f, 128.0f},
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .zIndex = -10
    });

    auto bgEntity2 = registry.create();
    registry.emplace<ParallaxComponent>(bgEntity2, ParallaxComponent{.scrollFactor = 0.5f, .baseOffset = glm::vec2(0.0f, 0.0f)});
    registry.emplace<SpriteComponent>(bgEntity2, SpriteComponent{
        .srcRect = SDL_FRect{256.0f, 256.0f, 256.0f, 128.0f},
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .zIndex = -9
    });

    // 2. Build Procedural Tilemap Layer
    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tileType = 0;

            // Ground floor
            if (y >= 17) {
                tileType = 1; // Dirt / Grass
            } else if (y == 16 && (x % 15 >= 3 && x % 15 <= 7) && x > 5) {
                tileType = 2; // Stone Platform
            } else if (y == 16 && x == 25) {
                tileType = 3; // Hazard Spikes
            }

            if (tileType == 0) continue;

            auto tileEntity = registry.create();
            glm::vec2 pos(x * tileSize, y * tileSize);

            registry.emplace<TransformComponent>(tileEntity, TransformComponent{.position = pos, .prevPosition = pos});
            registry.emplace<TileComponent>(tileEntity, TileComponent{.tileType = tileType});

            SDL_FRect srcRect{0.0f, 0.0f, 32.0f, 32.0f};
            if (tileType == 1) srcRect = SDL_FRect{32.0f, 0.0f, 32.0f, 32.0f};
            else if (tileType == 2) srcRect = SDL_FRect{64.0f, 0.0f, 32.0f, 32.0f};
            else if (tileType == 3) srcRect = SDL_FRect{96.0f, 0.0f, 32.0f, 32.0f};

            registry.emplace<SpriteComponent>(tileEntity, SpriteComponent{
                .srcRect = srcRect,
                .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                .zIndex = 0
            });

            // Create Static Box2D Physics Body for Ground & Platforms
            if (tileType == 1 || tileType == 2) {
                b2BodyDef bodyDef = b2DefaultBodyDef();
                bodyDef.type = b2_staticBody;
                bodyDef.position = (b2Vec2){(pos.x + 16.0f) / 32.0f, (pos.y + 16.0f) / 32.0f};

                b2BodyId bodyId = b2CreateBody(physicsWorld, &bodyDef);

                b2Polygon box = b2MakeBox(0.5f, 0.5f);
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.friction = 0.6f;
                b2CreatePolygonShape(bodyId, &shapeDef, &box);

                registry.emplace<RigidBodyComponent>(tileEntity, RigidBodyComponent{.bodyId = bodyId});
            }
        }
    }

    // 3. Create Player Entity
    auto playerEntity = registry.create();
    glm::vec2 playerStartPos(100.0f, 400.0f);

    registry.emplace<TransformComponent>(playerEntity, TransformComponent{
        .position = playerStartPos,
        .prevPosition = playerStartPos,
        .scale = glm::vec2(1.0f, 1.0f)
    });

    registry.emplace<SpriteComponent>(playerEntity, SpriteComponent{
        .srcRect = SDL_FRect{0.0f, 32.0f, 32.0f, 32.0f},
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .zIndex = 5
    });

    registry.emplace<AnimationComponent>(playerEntity, AnimationComponent{
        .currentFrame = 0,
        .totalFrames = 4,
        .frameDuration = 0.12f,
        .loop = true,
        .row = 0,
        .frameWidth = 32.0f,
        .frameHeight = 32.0f
    });

    registry.emplace<PlayerComponent>(playerEntity, PlayerComponent{});
    registry.emplace<StatsComponent>(playerEntity, StatsComponent{
        .hp = 100.0f,
        .maxHp = 100.0f,
        .mp = 50.0f,
        .maxMp = 50.0f,
        .level = 1,
        .attackPower = 25.0f,
        .moveSpeed = 220.0f
    });

    // Box2D Dynamic Body for Player
    b2BodyDef playerBodyDef = b2DefaultBodyDef();
    playerBodyDef.type = b2_dynamicBody;
    playerBodyDef.position = (b2Vec2){(playerStartPos.x + 16.0f) / 32.0f, (playerStartPos.y + 16.0f) / 32.0f};
    playerBodyDef.fixedRotation = true;

    b2BodyId playerBodyId = b2CreateBody(physicsWorld, &playerBodyDef);
    b2Polygon playerCapsule = b2MakeBox(0.35f, 0.45f);
    b2ShapeDef playerShapeDef = b2DefaultShapeDef();
    playerShapeDef.friction = 0.2f;
    b2CreatePolygonShape(playerBodyId, &playerShapeDef, &playerCapsule);

    registry.emplace<RigidBodyComponent>(playerEntity, RigidBodyComponent{.bodyId = playerBodyId});

    // 4. Create Enemy Entities (Melee Skeleton & Ranged Wizard)
    struct EnemySpawn {
        glm::vec2 pos;
        EnemyType type;
        float patrolRange;
    };

    EnemySpawn spawns[] = {
        { { 500.0f, 400.0f }, EnemyType::MeleeSkeleton, 150.0f },
        { { 900.0f, 400.0f }, EnemyType::RangedWizard, 100.0f },
        { { 1400.0f, 400.0f }, EnemyType::MeleeSkeleton, 200.0f },
        { { 1800.0f, 400.0f }, EnemyType::RangedWizard, 150.0f },
        { { 2300.0f, 400.0f }, EnemyType::MeleeSkeleton, 180.0f }
    };

    for (const auto& spawn : spawns) {
        auto enemyEntity = registry.create();

        registry.emplace<TransformComponent>(enemyEntity, TransformComponent{
            .position = spawn.pos,
            .prevPosition = spawn.pos
        });

        float rowY = (spawn.type == EnemyType::MeleeSkeleton) ? 96.0f : 128.0f;
        registry.emplace<SpriteComponent>(enemyEntity, SpriteComponent{
            .srcRect = SDL_FRect{0.0f, rowY, 32.0f, 32.0f},
            .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .zIndex = 4
        });

        registry.emplace<AnimationComponent>(enemyEntity, AnimationComponent{
            .currentFrame = 0,
            .totalFrames = 4,
            .frameDuration = 0.15f,
            .loop = true,
            .row = (spawn.type == EnemyType::MeleeSkeleton) ? 3 : 4,
            .frameWidth = 32.0f,
            .frameHeight = 32.0f
        });

        registry.emplace<EnemyComponent>(enemyEntity, EnemyComponent{
            .type = spawn.type,
            .state = AIState::Patrol,
            .patrolLeftX = spawn.pos.x - spawn.patrolRange,
            .patrolRightX = spawn.pos.x + spawn.patrolRange,
            .detectionRange = (spawn.type == EnemyType::RangedWizard) ? 350.0f : 200.0f,
            .attackRange = (spawn.type == EnemyType::RangedWizard) ? 250.0f : 45.0f
        });

        registry.emplace<StatsComponent>(enemyEntity, StatsComponent{
            .hp = (spawn.type == EnemyType::MeleeSkeleton) ? 60.0f : 40.0f,
            .maxHp = (spawn.type == EnemyType::MeleeSkeleton) ? 60.0f : 40.0f
        });

        // Box2D Dynamic Body for Enemy
        b2BodyDef eBodyDef = b2DefaultBodyDef();
        eBodyDef.type = b2_dynamicBody;
        eBodyDef.position = (b2Vec2){(spawn.pos.x + 16.0f) / 32.0f, (spawn.pos.y + 16.0f) / 32.0f};
        eBodyDef.fixedRotation = true;

        b2BodyId eBodyId = b2CreateBody(physicsWorld, &eBodyDef);
        b2Polygon eBox = b2MakeBox(0.35f, 0.45f);
        b2ShapeDef eShapeDef = b2DefaultShapeDef();
        eShapeDef.friction = 0.3f;
        b2CreatePolygonShape(eBodyId, &eShapeDef, &eBox);

        registry.emplace<RigidBodyComponent>(enemyEntity, RigidBodyComponent{.bodyId = eBodyId});
    }

    LOG_INFO("World initialized successfully: 100x20 Tilemap, Player, and Enemies created.");

    // =========================================================================
    // MAIN GAME LOOP (Fixed Timestep 60Hz + Interpolation, Zero Allocations)
    // =========================================================================
    TimeStep timeStep(1.0 / 60.0);
    bool quit = false;
    int frameCounter = 0;

    LOG_INFO("Entering zero-stutter main game loop...");

    while (!quit) {
        SIRPG_PROFILE_FRAME();
        timeStep.tick();

        // 1. Process Input
        quit = InputSystem::pollEvents(registry);

        // 2. Fixed Timestep Physics & Game Logic Steps
        while (timeStep.checkStep()) {
            float fixedDT = timeStep.getFixedDeltaTime();

            PlayerSystem::update(registry, fixedDT, particlePool);
            AISystem::update(registry, fixedDT, physicsWorld);
            PhysicsSystem::update(registry, physicsWorld, fixedDT);
            CombatSystem::update(registry, fixedDT, floatingTextPool, particlePool);
            ParticleSystem::update(particlePool, fixedDT);
            AnimationSystem::update(registry, fixedDT);
        }

        // 3. Smooth Camera Tracking on Player Position
        auto pTransform = registry.get<TransformComponent>(playerEntity);
        camera.update(pTransform.position, timeStep.getFrameTime());

        // 4. Render Pipeline
        RenderSystem::render(
            renderer,
            registry,
            batchedRenderer,
            textureAtlas,
            camera,
            timeStep.getAlpha(),
            floatingTextPool,
            particlePool
        );

        frameCounter++;

        // Stop automatically in headless mode after 300 frames if running headless test
        const char* headlessEnv = std::getenv("HEADLESS_RUN");
        if (headlessEnv && frameCounter >= 300) {
            LOG_INFO("Headless test run complete ({} frames processed). Exiting.", frameCounter);
            quit = true;
        }
    }

    // Cleanup physics world entities and world
    auto view = registry.view<RigidBodyComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBodyComponent>(entity);
        if (b2Body_IsValid(rb.bodyId)) {
            b2DestroyBody(rb.bodyId);
            rb.bodyId = b2_nullBodyId;
        }
    }

    b2DestroyWorld(physicsWorld);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    LOG_INFO("Engine shutdown cleanly.");
    return 0;
}
