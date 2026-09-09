#ifndef SIRPG_SYSTEMS_HPP
#define SIRPG_SYSTEMS_HPP

#include <entt/entt.hpp>
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <box2d/math_functions.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <span>

#include "../Components/Components.hpp"
#include "../Core/ObjectPool.hpp"
#include "../Core/Logger.hpp"
#include "../Core/Profiler.hpp"
#include "../Engine/BatchedRenderer.hpp"
#include "../Engine/TextureAtlas.hpp"

namespace sirpg::systems {

using namespace components;
using namespace engine;

// Helper to safely destroy an entity and cleanup its Box2D body if present
inline void destroyEntitySafely(entt::registry& registry, entt::entity entity) {
    if (registry.valid(entity)) {
        if (auto* rb = registry.try_get<RigidBodyComponent>(entity)) {
            if (b2Body_IsValid(rb->bodyId)) {
                b2DestroyBody(rb->bodyId);
                rb->bodyId = b2_nullBodyId;
            }
        }
        registry.destroy(entity);
    }
}

// -----------------------------------------------------------------------------
// 1. INPUT SYSTEM
// -----------------------------------------------------------------------------
class InputSystem {
public:
    static bool pollEvents(entt::registry& registry) {
        SIRPG_PROFILE_ZONE();
        SDL_Event event;
        bool quit = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit = true;
            }
        }

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        auto view = registry.view<PlayerComponent>();

        for (auto entity : view) {
            auto& player = view.get<PlayerComponent>(entity);

            player.moveLeft = keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT];
            player.moveRight = keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT];
            player.wantJump = keyboard[SDL_SCANCODE_SPACE] || keyboard[SDL_SCANCODE_W] || keyboard[SDL_SCANCODE_UP];
            player.wantAttack = keyboard[SDL_SCANCODE_J] || keyboard[SDL_SCANCODE_Z];
            player.wantFireball = keyboard[SDL_SCANCODE_K] || keyboard[SDL_SCANCODE_X];
        }

        return quit;
    }
};

// -----------------------------------------------------------------------------
// 2. CAMERA SYSTEM
// -----------------------------------------------------------------------------
struct Camera {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 viewportSize{1280.0f, 720.0f};
    glm::vec2 worldBoundsMin{0.0f, 0.0f};
    glm::vec2 worldBoundsMax{3200.0f, 640.0f}; // 100 tiles x 32px wide = 3200
    float smoothSpeed{5.0f};

    void update(const glm::vec2& targetPos, float deltaTime) {
        SIRPG_PROFILE_ZONE();
        glm::vec2 desiredPos = targetPos - viewportSize * 0.5f;

        // Smooth Damp / Lerp
        position += (desiredPos - position) * std::min(smoothSpeed * deltaTime, 1.0f);

        // Clamp to map boundaries
        position.x = std::clamp(position.x, worldBoundsMin.x, worldBoundsMax.x - viewportSize.x);
        position.y = std::clamp(position.y, worldBoundsMin.y, worldBoundsMax.y - viewportSize.y);
    }
};

// -----------------------------------------------------------------------------
// 3. PHYSICS SYSTEM
// -----------------------------------------------------------------------------
class PhysicsSystem {
public:
    static b2WorldId createWorld() {
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = (b2Vec2){0.0f, 25.0f}; // Box2D v3 meters/sec^2
        return b2CreateWorld(&worldDef);
    }

    static void update(entt::registry& registry, b2WorldId worldId, float deltaTime) {
        SIRPG_PROFILE_ZONE();
        // Step Box2D v3 simulation
        int subStepCount = 4;
        b2World_Step(worldId, deltaTime, subStepCount);

        // Synchronize Box2D body positions back to TransformComponent
        auto view = registry.view<TransformComponent, RigidBodyComponent>();
        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& rb = view.get<RigidBodyComponent>(entity);

            if (b2Body_IsValid(rb.bodyId)) {
                b2Vec2 pos = b2Body_GetPosition(rb.bodyId);
                b2Rot rot = b2Body_GetRotation(rb.bodyId);

                // Save previous position for rendering interpolation
                transform.prevPosition = transform.position;
                transform.prevRotation = transform.rotation;

                // Box2D center position in meters -> engine top-left sprite position in pixels
                // Sprite size is 32x32 (half-width = 16px)
                transform.position = glm::vec2(pos.x * 32.0f - 16.0f, pos.y * 32.0f - 16.0f);
                transform.rotation = b2Rot_GetAngle(rot);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// 4. ANIMATION SYSTEM
// -----------------------------------------------------------------------------
class AnimationSystem {
public:
    static void update(entt::registry& registry, float deltaTime) {
        SIRPG_PROFILE_ZONE();
        auto view = registry.view<AnimationComponent, SpriteComponent>();
        for (auto entity : view) {
            auto& anim = view.get<AnimationComponent>(entity);
            auto& sprite = view.get<SpriteComponent>(entity);

            if (anim.totalFrames <= 1) continue;

            anim.elapsedTime += deltaTime;
            if (anim.elapsedTime >= anim.frameDuration) {
                anim.elapsedTime -= anim.frameDuration;
                anim.currentFrame++;

                if (anim.currentFrame >= anim.totalFrames) {
                    if (anim.loop) {
                        anim.currentFrame = 0;
                    } else {
                        anim.currentFrame = anim.totalFrames - 1;
                        anim.finished = true;
                    }
                }
            }

            // Shift texture srcRect based on current frame
            sprite.srcRect.x = static_cast<float>(anim.currentFrame * anim.frameWidth);
            sprite.srcRect.y = static_cast<float>(anim.row * anim.frameHeight);
            sprite.srcRect.w = anim.frameWidth;
            sprite.srcRect.h = anim.frameHeight;
        }
    }
};

// -----------------------------------------------------------------------------
// 5. PARTICLE SYSTEM (Zero-Allocation Visual FX Pool)
// -----------------------------------------------------------------------------
class ParticleSystem {
public:
    static void update(
        sirpg::core::ObjectPool<Particle, 256>& particlePool,
        float deltaTime
    ) {
        SIRPG_PROFILE_ZONE();
        particlePool.forEachActive([deltaTime, &particlePool](Particle& p) {
            p.elapsedTime += deltaTime;
            p.position += p.velocity * deltaTime;
            p.color.a = 1.0f - (p.elapsedTime / p.lifetime);

            if (p.elapsedTime >= p.lifetime) {
                particlePool.recycle(&p);
            }
        });
    }

    static void spawnBurst(
        sirpg::core::ObjectPool<Particle, 256>& particlePool,
        const glm::vec2& origin,
        const glm::vec4& color,
        int count = 8
    ) {
        for (int i = 0; i < count; ++i) {
            auto pRes = particlePool.spawn();
            if (pRes) {
                Particle* p = pRes.value();
                float angle = static_cast<float>(i) * (2.0f * 3.14159f / static_cast<float>(count));
                float speed = 80.0f + static_cast<float>(i * 15 % 50);

                p->position = origin;
                p->velocity = glm::vec2(std::cos(angle) * speed, std::sin(angle) * speed);
                p->size = glm::vec2(6.0f, 6.0f);
                p->color = color;
                p->lifetime = 0.4f;
                p->elapsedTime = 0.0f;
                p->srcRect = SDL_FRect{160.0f, 160.0f, 16.0f, 16.0f};
            }
        }
    }
};

// -----------------------------------------------------------------------------
// 6. AI SYSTEM
// -----------------------------------------------------------------------------
class AISystem {
public:
    static void update(entt::registry& registry, float deltaTime, b2WorldId worldId) {
        SIRPG_PROFILE_ZONE();
        (void)worldId;
        // Find player position
        glm::vec2 playerPos{0.0f, 0.0f};
        auto playerView = registry.view<PlayerComponent, TransformComponent>();
        for (auto pEntity : playerView) {
            playerPos = playerView.get<TransformComponent>(pEntity).position;
            break;
        }

        auto enemyView = registry.view<EnemyComponent, TransformComponent, RigidBodyComponent, SpriteComponent, AnimationComponent>();
        for (auto entity : enemyView) {
            auto& enemy = enemyView.get<EnemyComponent>(entity);
            auto& transform = enemyView.get<TransformComponent>(entity);
            auto& rb = enemyView.get<RigidBodyComponent>(entity);
            auto& sprite = enemyView.get<SpriteComponent>(entity);

            if (enemy.state == AIState::Dead) continue;

            float distToPlayer = glm::distance(transform.position, playerPos);

            if (enemy.attackCooldown > 0.0f) {
                enemy.attackCooldown -= deltaTime;
            }

            // FSM Transitions
            if (distToPlayer <= enemy.attackRange) {
                enemy.state = AIState::Attack;
            } else if (distToPlayer <= enemy.detectionRange) {
                enemy.state = AIState::Chase;
            } else {
                enemy.state = AIState::Patrol;
            }

            b2Vec2 currentVel = b2Body_GetLinearVelocity(rb.bodyId);
            float moveVelX = 0.0f;

            switch (enemy.state) {
                case AIState::Patrol: {
                    if (transform.position.x <= enemy.patrolLeftX) {
                        enemy.moveDirection = 1;
                    } else if (transform.position.x >= enemy.patrolRightX) {
                        enemy.moveDirection = -1;
                    }
                    moveVelX = static_cast<float>(enemy.moveDirection) * 2.0f; // 2 m/s
                    sprite.flipHorizontal = (enemy.moveDirection < 0);
                    break;
                }
                case AIState::Chase: {
                    if (playerPos.x < transform.position.x) {
                        enemy.moveDirection = -1;
                    } else {
                        enemy.moveDirection = 1;
                    }
                    moveVelX = static_cast<float>(enemy.moveDirection) * 3.5f; // 3.5 m/s
                    sprite.flipHorizontal = (enemy.moveDirection < 0);
                    break;
                }
                case AIState::Attack: {
                    moveVelX = 0.0f;
                    sprite.flipHorizontal = (playerPos.x < transform.position.x);

                    if (enemy.attackCooldown <= 0.0f) {
                        enemy.attackCooldown = enemy.attackInterval;
                        // Trigger attack
                        if (enemy.type == EnemyType::MeleeSkeleton) {
                            // Melee skeleton attack hitbox entity
                            auto hitboxEntity = registry.create();
                            registry.emplace<TransformComponent>(hitboxEntity, transform.position + glm::vec2(enemy.moveDirection * 24.0f, 0.0f));
                            registry.emplace<HitboxComponent>(hitboxEntity, HitboxComponent{
                                .offset = glm::vec2(0.0f),
                                .size = glm::vec2(32.0f, 32.0f),
                                .damage = 15.0f,
                                .lifetime = 0.2f,
                                .elapsedTime = 0.0f,
                                .active = true,
                                .ownerEntityId = static_cast<uint32_t>(entity),
                                .isPlayerAttack = false
                            });
                        } else if (enemy.type == EnemyType::RangedWizard) {
                            // Ranged wizard projectile entity
                            auto projEntity = registry.create();
                            glm::vec2 projDir = glm::normalize(playerPos - transform.position);
                            registry.emplace<TransformComponent>(projEntity, transform.position + projDir * 20.0f);
                            registry.emplace<SpriteComponent>(projEntity, SpriteComponent{
                                .srcRect = SDL_FRect{192.0f, 160.0f, 32.0f, 32.0f},
                                .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                .zIndex = 2
                            });
                            registry.emplace<ProjectileComponent>(projEntity, ProjectileComponent{
                                .velocity = projDir * 250.0f,
                                .damage = 12.0f,
                                .lifetime = 4.0f,
                                .elapsedTime = 0.0f,
                                .isFromPlayer = false
                            });
                        }
                    }
                    break;
                }
                case AIState::Dead:
                    break;
            }

            // Set linear velocity in Box2D body
            b2Body_SetLinearVelocity(rb.bodyId, (b2Vec2){moveVelX, currentVel.y});
        }
    }
};

// -----------------------------------------------------------------------------
// 7. COMBAT & PROJECTILE SYSTEM
// -----------------------------------------------------------------------------
class CombatSystem {
public:
    static void update(
        entt::registry& registry,
        float deltaTime,
        sirpg::core::ObjectPool<FloatingText, 128>& floatingTextPool,
        sirpg::core::ObjectPool<Particle, 256>& particlePool
    ) {
        SIRPG_PROFILE_ZONE();

        // Update Floating Damage Texts
        floatingTextPool.forEachActive([deltaTime, &floatingTextPool](FloatingText& text) {
            text.elapsedTime += deltaTime;
            text.position.y -= 30.0f * deltaTime; // Float upward
            text.color.a = 1.0f - (text.elapsedTime / text.lifetime);

            if (text.elapsedTime >= text.lifetime) {
                floatingTextPool.recycle(&text);
            }
        });

        // Buffer for deferred entity destruction to avoid invalidating EnTT views
        thread_local std::vector<entt::entity> pendingDestroy;
        pendingDestroy.clear();

        // 1. Update Projectiles
        auto projView = registry.view<ProjectileComponent, TransformComponent>();
        for (auto entity : projView) {
            auto& proj = projView.get<ProjectileComponent>(entity);
            auto& transform = projView.get<TransformComponent>(entity);

            proj.elapsedTime += deltaTime;
            transform.position += proj.velocity * deltaTime;

            if (proj.elapsedTime >= proj.lifetime) {
                pendingDestroy.push_back(entity);
                continue;
            }

            // Check projectile collisions with entities
            if (proj.isFromPlayer) {
                // Check against enemies
                auto enemyView = registry.view<EnemyComponent, TransformComponent, StatsComponent>();
                for (auto eEntity : enemyView) {
                    auto& eStats = enemyView.get<StatsComponent>(eEntity);
                    auto& eTransform = enemyView.get<TransformComponent>(eEntity);

                    float dist = glm::distance(transform.position, eTransform.position);
                    if (dist < 28.0f) { // Collision hit
                        eStats.hp -= proj.damage;

                        // Particle burst on magic hit
                        ParticleSystem::spawnBurst(particlePool, eTransform.position, glm::vec4(1.0f, 0.7f, 0.1f, 1.0f), 10);

                        // Spawn floating damage text from pool
                        auto textRes = floatingTextPool.spawn();
                        if (textRes) {
                            FloatingText* text = textRes.value();
                            text->position = eTransform.position + glm::vec2(0.0f, -20.0f);
                            text->damageValue = proj.damage;
                            text->color = glm::vec4(1.0f, 0.85f, 0.0f, 1.0f); // Yellow for player magic
                            text->lifetime = 0.8f;
                            text->elapsedTime = 0.0f;
                            std::snprintf(text->textBuffer, sizeof(text->textBuffer), "-%.0f", proj.damage);
                        }

                        if (eStats.hp <= 0.0f) {
                            enemyView.get<EnemyComponent>(eEntity).state = AIState::Dead;
                            pendingDestroy.push_back(eEntity);
                        }

                        pendingDestroy.push_back(entity); // Destroy projectile
                        break;
                    }
                }
            } else {
                // Enemy projectile hitting player
                auto playerView = registry.view<PlayerComponent, TransformComponent, StatsComponent>();
                for (auto pEntity : playerView) {
                    auto& pStats = playerView.get<StatsComponent>(pEntity);
                    auto& pTransform = playerView.get<TransformComponent>(pEntity);

                    float dist = glm::distance(transform.position, pTransform.position);
                    if (dist < 24.0f) {
                        pStats.hp = std::max(0.0f, pStats.hp - proj.damage);

                        // Particle burst on player hit
                        ParticleSystem::spawnBurst(particlePool, pTransform.position, glm::vec4(0.8f, 0.1f, 0.8f, 1.0f), 8);

                        // Floating damage text
                        auto textRes = floatingTextPool.spawn();
                        if (textRes) {
                            FloatingText* text = textRes.value();
                            text->position = pTransform.position + glm::vec2(0.0f, -20.0f);
                            text->damageValue = proj.damage;
                            text->color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // Red for taken damage
                            text->lifetime = 0.8f;
                            text->elapsedTime = 0.0f;
                            std::snprintf(text->textBuffer, sizeof(text->textBuffer), "-%.0f", proj.damage);
                        }

                        pendingDestroy.push_back(entity);
                        break;
                    }
                }
            }
        }

        // 2. Update Hitboxes
        auto hitboxView = registry.view<HitboxComponent, TransformComponent>();
        for (auto entity : hitboxView) {
            auto& hitbox = hitboxView.get<HitboxComponent>(entity);
            auto& hTransform = hitboxView.get<TransformComponent>(entity);

            hitbox.elapsedTime += deltaTime;

            if (hitbox.isPlayerAttack) {
                // Check enemy targets
                auto enemyView = registry.view<EnemyComponent, TransformComponent, StatsComponent>();
                for (auto eEntity : enemyView) {
                    auto& eStats = enemyView.get<StatsComponent>(eEntity);
                    auto& eTransform = enemyView.get<TransformComponent>(eEntity);

                    float dist = glm::distance(hTransform.position, eTransform.position);
                    if (dist < 36.0f && hitbox.active) {
                        eStats.hp -= hitbox.damage;
                        hitbox.active = false; // Only hit once per attack

                        // Particle burst on melee hit
                        ParticleSystem::spawnBurst(particlePool, eTransform.position, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 6);

                        auto textRes = floatingTextPool.spawn();
                        if (textRes) {
                            FloatingText* text = textRes.value();
                            text->position = eTransform.position + glm::vec2(0.0f, -20.0f);
                            text->damageValue = hitbox.damage;
                            text->color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White for melee
                            text->lifetime = 0.8f;
                            text->elapsedTime = 0.0f;
                            std::snprintf(text->textBuffer, sizeof(text->textBuffer), "-%.0f", hitbox.damage);
                        }

                        if (eStats.hp <= 0.0f) {
                            enemyView.get<EnemyComponent>(eEntity).state = AIState::Dead;
                            pendingDestroy.push_back(eEntity);
                        }
                    }
                }
            } else {
                // Enemy melee attack hitting player
                auto playerView = registry.view<PlayerComponent, TransformComponent, StatsComponent>();
                for (auto pEntity : playerView) {
                    auto& pStats = playerView.get<StatsComponent>(pEntity);
                    auto& pTransform = playerView.get<TransformComponent>(pEntity);

                    float dist = glm::distance(hTransform.position, pTransform.position);
                    if (dist < 32.0f && hitbox.active) {
                        pStats.hp = std::max(0.0f, pStats.hp - hitbox.damage);
                        hitbox.active = false;

                        ParticleSystem::spawnBurst(particlePool, pTransform.position, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f), 8);

                        auto textRes = floatingTextPool.spawn();
                        if (textRes) {
                            FloatingText* text = textRes.value();
                            text->position = pTransform.position + glm::vec2(0.0f, -20.0f);
                            text->damageValue = hitbox.damage;
                            text->color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
                            text->lifetime = 0.8f;
                            text->elapsedTime = 0.0f;
                            std::snprintf(text->textBuffer, sizeof(text->textBuffer), "-%.0f", hitbox.damage);
                        }
                    }
                }
            }

            if (hitbox.elapsedTime >= hitbox.lifetime) {
                pendingDestroy.push_back(entity);
            }
        }

        // Process deferred entity destructions safely and cleanup physics bodies
        for (auto entity : pendingDestroy) {
            destroyEntitySafely(registry, entity);
        }
    }
};

// -----------------------------------------------------------------------------
// 8. PLAYER CONTROLLER SYSTEM
// -----------------------------------------------------------------------------
class PlayerSystem {
public:
    static void update(
        entt::registry& registry,
        float deltaTime,
        sirpg::core::ObjectPool<Particle, 256>& particlePool
    ) {
        SIRPG_PROFILE_ZONE();
        auto view = registry.view<PlayerComponent, TransformComponent, RigidBodyComponent, StatsComponent, SpriteComponent, AnimationComponent>();

        for (auto entity : view) {
            auto& player = view.get<PlayerComponent>(entity);
            auto& transform = view.get<TransformComponent>(entity);
            auto& rb = view.get<RigidBodyComponent>(entity);
            auto& stats = view.get<StatsComponent>(entity);
            auto& sprite = view.get<SpriteComponent>(entity);
            auto& anim = view.get<AnimationComponent>(entity);

            b2Vec2 currentVel = b2Body_GetLinearVelocity(rb.bodyId);

            // Ground check (if vertical velocity is close to zero)
            player.isGrounded = std::abs(currentVel.y) < 0.1f;
            if (player.isGrounded) {
                player.jumpsRemaining = player.maxJumps;
            }

            // Cooldowns
            if (player.attackCooldown > 0.0f) player.attackCooldown -= deltaTime;
            if (player.fireballCooldown > 0.0f) player.fireballCooldown -= deltaTime;

            // Passive MP regeneration
            stats.mp = std::min(stats.maxMp, stats.mp + 5.0f * deltaTime);

            // Movement Logic
            float desiredVelX = 0.0f;
            if (player.moveLeft) {
                desiredVelX = -stats.moveSpeed / 32.0f; // in meters/sec
                sprite.flipHorizontal = true;
            } else if (player.moveRight) {
                desiredVelX = stats.moveSpeed / 32.0f;
                sprite.flipHorizontal = false;
            }

            // Handle Jump (Double Jump)
            static bool prevJumpState = false;
            if (player.wantJump && !prevJumpState && player.jumpsRemaining > 0) {
                currentVel.y = -12.0f; // Impulse upward velocity
                player.jumpsRemaining--;

                // Jump Dust Particles
                ParticleSystem::spawnBurst(particlePool, transform.position + glm::vec2(16.0f, 32.0f), glm::vec4(0.8f, 0.8f, 0.8f, 0.8f), 6);
            }
            prevJumpState = player.wantJump;

            // Set Box2D velocity
            b2Body_SetLinearVelocity(rb.bodyId, (b2Vec2){desiredVelX, currentVel.y});

            // Handle Melee Attack
            if (player.wantAttack && player.attackCooldown <= 0.0f) {
                player.attackCooldown = 0.35f;
                anim.row = 1; // Melee swing row
                anim.currentFrame = 0;

                // Spawn melee attack hitbox entity
                float attackDir = sprite.flipHorizontal ? -1.0f : 1.0f;
                auto hitboxEntity = registry.create();
                registry.emplace<TransformComponent>(hitboxEntity, transform.position + glm::vec2(attackDir * 28.0f, 0.0f));
                registry.emplace<HitboxComponent>(hitboxEntity, HitboxComponent{
                    .offset = glm::vec2(0.0f),
                    .size = glm::vec2(36.0f, 32.0f),
                    .damage = stats.attackPower,
                    .lifetime = 0.2f,
                    .elapsedTime = 0.0f,
                    .active = true,
                    .ownerEntityId = static_cast<uint32_t>(entity),
                    .isPlayerAttack = true
                });
            } else if (anim.row == 1 && anim.finished) {
                anim.row = 0; // Return to idle row
            }

            // Handle Ranged Fireball (Requires 10 MP)
            if (player.wantFireball && player.fireballCooldown <= 0.0f && stats.mp >= 10.0f) {
                stats.mp -= 10.0f;
                player.fireballCooldown = 0.5f;

                float projDir = sprite.flipHorizontal ? -1.0f : 1.0f;
                auto projEntity = registry.create();
                registry.emplace<TransformComponent>(projEntity, transform.position + glm::vec2(projDir * 24.0f, 0.0f));
                registry.emplace<SpriteComponent>(projEntity, SpriteComponent{
                    .srcRect = SDL_FRect{160.0f, 160.0f, 32.0f, 32.0f},
                    .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                    .zIndex = 2
                });
                registry.emplace<ProjectileComponent>(projEntity, ProjectileComponent{
                    .velocity = glm::vec2(projDir * 400.0f, 0.0f),
                    .damage = 35.0f,
                    .lifetime = 3.0f,
                    .elapsedTime = 0.0f,
                    .isFromPlayer = true
                });

                ParticleSystem::spawnBurst(particlePool, transform.position + glm::vec2(projDir * 24.0f, 16.0f), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 8);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// 9. RENDER SYSTEM
// -----------------------------------------------------------------------------
class RenderSystem {
public:
    static void render(
        SDL_Renderer* renderer,
        entt::registry& registry,
        BatchedRenderer& batchedRenderer,
        TextureAtlas& textureAtlas,
        const Camera& camera,
        float interpolationAlpha,
        sirpg::core::ObjectPool<FloatingText, 128>& floatingTextPool,
        sirpg::core::ObjectPool<Particle, 256>& particlePool
    ) {
        SIRPG_PROFILE_ZONE();
        SDL_SetRenderDrawColor(renderer, 15, 15, 30, 255); // Dark blue night background
        SDL_RenderClear(renderer);

        batchedRenderer.beginBatch();

        // 1. Render Parallax Layers (Repeated Horizontally Across Scrolling Map)
        auto parallaxView = registry.view<ParallaxComponent, SpriteComponent>();
        for (auto entity : parallaxView) {
            auto& parallax = parallaxView.get<ParallaxComponent>(entity);
            auto& sprite = parallaxView.get<SpriteComponent>(entity);

            float layerWidth = 1280.0f;
            float layerOffsetX = std::fmod(camera.position.x * parallax.scrollFactor, layerWidth);

            for (float x = -layerOffsetX - layerWidth; x < camera.viewportSize.x + layerWidth; x += layerWidth) {
                SDL_FRect dstRect{
                    x + parallax.baseOffset.x,
                    parallax.baseOffset.y,
                    layerWidth,
                    720.0f
                };

                batchedRenderer.submitQuad(RenderQuadCommand{
                    .srcRect = sprite.srcRect,
                    .dstRect = dstRect,
                    .color = sprite.color,
                    .rotation = 0.0f,
                    .zIndex = sprite.zIndex
                });
            }
        }

        // 2. Render Tilemap with Frustum Culling
        auto tileView = registry.view<TileComponent, TransformComponent, SpriteComponent>();
        for (auto entity : tileView) {
            auto& transform = tileView.get<TransformComponent>(entity);
            auto& sprite = tileView.get<SpriteComponent>(entity);

            // Interpolate position
            glm::vec2 interpolatedPos = glm::mix(transform.prevPosition, transform.position, interpolationAlpha);

            // World -> Screen Coordinates
            glm::vec2 screenPos = interpolatedPos - camera.position;

            // Frustum Culling Check
            if (screenPos.x + 32.0f < 0 || screenPos.x > camera.viewportSize.x ||
                screenPos.y + 32.0f < 0 || screenPos.y > camera.viewportSize.y) {
                continue; // Skip rendering out-of-screen tiles
            }

            batchedRenderer.submitQuad(RenderQuadCommand{
                .srcRect = sprite.srcRect,
                .dstRect = SDL_FRect{screenPos.x, screenPos.y, 32.0f, 32.0f},
                .color = sprite.color,
                .rotation = 0.0f,
                .zIndex = sprite.zIndex
            });
        }

        // 3. Render Game Entities (Player, Enemies, Projectiles)
        auto entityView = registry.view<TransformComponent, SpriteComponent>();
        for (auto entity : entityView) {
            if (registry.all_of<TileComponent, ParallaxComponent>(entity)) continue;

            auto& transform = entityView.get<TransformComponent>(entity);
            auto& sprite = entityView.get<SpriteComponent>(entity);

            glm::vec2 interpolatedPos = glm::mix(transform.prevPosition, transform.position, interpolationAlpha);
            glm::vec2 screenPos = interpolatedPos - camera.position;

            batchedRenderer.submitQuad(RenderQuadCommand{
                .srcRect = sprite.srcRect,
                .dstRect = SDL_FRect{screenPos.x, screenPos.y, 32.0f * transform.scale.x, 32.0f * transform.scale.y},
                .color = sprite.color,
                .rotation = transform.rotation,
                .zIndex = sprite.zIndex,
                .flipHorizontal = sprite.flipHorizontal,
                .flipVertical = sprite.flipVertical
            });
        }

        // 4. Render Active FX Particles
        particlePool.forEachActive([&batchedRenderer, &camera](const Particle& p) {
            glm::vec2 screenPos = p.position - camera.position;
            batchedRenderer.submitQuad(RenderQuadCommand{
                .srcRect = p.srcRect,
                .dstRect = SDL_FRect{screenPos.x, screenPos.y, p.size.x, p.size.y},
                .color = p.color,
                .rotation = 0.0f,
                .zIndex = 10
            });
        });

        // Flush Batched Quads to GPU
        batchedRenderer.flush(renderer, textureAtlas.getTexture());

        // 5. Render UI / HUD (HP / MP Bars & Floating Damage Numbers)
        // Find Player Stats
        auto playerView = registry.view<PlayerComponent, StatsComponent>();
        for (auto pEntity : playerView) {
            auto& stats = playerView.get<StatsComponent>(pEntity);

            // HP Bar Background
            SDL_FRect hpBg{20.0f, 20.0f, 200.0f, 20.0f};
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &hpBg);

            // HP Bar Fill (Interpolated Red)
            float hpPct = std::clamp(stats.hp / stats.maxHp, 0.0f, 1.0f);
            SDL_FRect hpFill{20.0f, 20.0f, 200.0f * hpPct, 20.0f};
            SDL_SetRenderDrawColor(renderer, 220, 20, 60, 255);
            SDL_RenderFillRect(renderer, &hpFill);

            // MP Bar Background
            SDL_FRect mpBg{20.0f, 45.0f, 150.0f, 15.0f};
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &mpBg);

            // MP Bar Fill (Blue)
            float mpPct = std::clamp(stats.mp / stats.maxMp, 0.0f, 1.0f);
            SDL_FRect mpFill{20.0f, 45.0f, 150.0f * mpPct, 15.0f};
            SDL_SetRenderDrawColor(renderer, 30, 144, 255, 255);
            SDL_RenderFillRect(renderer, &mpFill);

            // Text overlay using SDL_RenderDebugText
            char hudText[64];
            std::snprintf(hudText, sizeof(hudText), "HP: %.0f/%.0f  MP: %.0f/%.0f", stats.hp, stats.maxHp, stats.mp, stats.maxMp);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, 25.0f, 23.0f, hudText);
        }

        // Floating Damage Texts
        floatingTextPool.forEachActive([renderer, &camera](const FloatingText& text) {
            glm::vec2 screenPos = text.position - camera.position;
            SDL_SetRenderDrawColor(
                renderer,
                static_cast<Uint8>(text.color.r * 255.0f),
                static_cast<Uint8>(text.color.g * 255.0f),
                static_cast<Uint8>(text.color.b * 255.0f),
                static_cast<Uint8>(text.color.a * 255.0f)
            );
            SDL_RenderDebugText(renderer, screenPos.x, screenPos.y, text.textBuffer);
        });

        SDL_RenderPresent(renderer);
    }
};

} // namespace sirpg::systems

#endif // SIRPG_SYSTEMS_HPP
