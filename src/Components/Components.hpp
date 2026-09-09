#ifndef SIRPG_COMPONENTS_HPP
#define SIRPG_COMPONENTS_HPP

#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <box2d/id.h>
#include <span>

namespace sirpg::components {

// 1. Transform Component
struct TransformComponent {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 prevPosition{0.0f, 0.0f}; // For state interpolation (Lerp)
    glm::vec2 scale{1.0f, 1.0f};
    float rotation{0.0f};
    float prevRotation{0.0f};
};

// 2. Velocity Component
struct VelocityComponent {
    glm::vec2 linearVelocity{0.0f, 0.0f};
    float angularVelocity{0.0f};
};

// 3. Texture / Sprite Handle
using TextureHandle = SDL_Texture*;

struct SpriteComponent {
    TextureHandle texture{nullptr};
    SDL_FRect srcRect{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    int zIndex{0};
    bool flipHorizontal{false};
    bool flipVertical{false};
};

// 4. RigidBody Component (Box2D v3 b2BodyId)
struct RigidBodyComponent {
    b2BodyId bodyId{b2_nullBodyId};
};

// 5. BoxCollider Component
struct BoxColliderComponent {
    glm::vec2 size{32.0f, 32.0f};
    glm::vec2 offset{0.0f, 0.0f};
    bool isSensor{false};
    b2ShapeId shapeId{b2_nullShapeId};
};

// 6. Player Control Component
struct PlayerComponent {
    bool moveLeft{false};
    bool moveRight{false};
    bool wantJump{false};
    bool wantAttack{false};
    bool wantFireball{false};

    int jumpsRemaining{2};
    int maxJumps{2};
    bool isGrounded{false};

    float attackCooldown{0.0f};
    float fireballCooldown{0.0f};
};

// 7. Stats Component (RPG Attributes)
struct StatsComponent {
    float hp{100.0f};
    float maxHp{100.0f};
    float mp{50.0f};
    float maxMp{50.0f};
    int level{1};
    float exp{0.0f};
    float attackPower{20.0f};
    float moveSpeed{180.0f};
};

// 8. Animation Component
struct AnimationComponent {
    int currentFrame{0};
    int totalFrames{1};
    float frameDuration{0.1f};
    float elapsedTime{0.0f};
    bool loop{true};
    bool finished{false};
    int row{0};
    float frameWidth{32.0f};
    float frameHeight{32.0f};
};

// 9. Combat Hitbox & Hurtbox
struct HitboxComponent {
    glm::vec2 offset{0.0f, 0.0f};
    glm::vec2 size{32.0f, 32.0f};
    float damage{20.0f};
    float lifetime{0.2f};
    float elapsedTime{0.0f};
    bool active{true};
    uint32_t ownerEntityId{0};
    bool isPlayerAttack{true};
};

struct HurtboxComponent {
    glm::vec2 offset{0.0f, 0.0f};
    glm::vec2 size{32.0f, 32.0f};
    float damageMultiplier{1.0f};
    float invulnerabilityTimer{0.0f};
};

// 10. AI Enemy State
enum class AIState {
    Patrol,
    Chase,
    Attack,
    Dead
};

enum class EnemyType {
    MeleeSkeleton,
    RangedWizard
};

struct EnemyComponent {
    EnemyType type{EnemyType::MeleeSkeleton};
    AIState state{AIState::Patrol};
    float patrolLeftX{0.0f};
    float patrolRightX{0.0f};
    int moveDirection{1}; // 1 or -1
    float detectionRange{250.0f};
    float attackRange{50.0f};
    float attackCooldown{0.0f};
    float attackInterval{1.2f};
};

// 11. Projectile Component
struct ProjectileComponent {
    glm::vec2 velocity{0.0f, 0.0f};
    float damage{25.0f};
    float lifetime{3.0f};
    float elapsedTime{0.0f};
    bool isFromPlayer{true};
};

// 12. Tilemap Marker Component
struct TileComponent {
    int tileType{0}; // 0 = air, 1 = ground, 2 = platform, 3 = hazard
};

// 13. Parallax Layer Component
struct ParallaxComponent {
    float scrollFactor{0.5f}; // %20, %50, %100 speed
    glm::vec2 baseOffset{0.0f, 0.0f};
};

// 14. Floating Damage Text Pool Structure
struct FloatingText {
    glm::vec2 position{0.0f, 0.0f};
    float damageValue{0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float lifetime{1.0f};
    float elapsedTime{0.0f};
    bool active{false};
    char textBuffer[32]{};
};

// 15. Particle Pool Structure for FX (Zero-Allocation Particles)
struct Particle {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec2 size{8.0f, 8.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float lifetime{0.5f};
    float elapsedTime{0.0f};
    SDL_FRect srcRect{160.0f, 160.0f, 16.0f, 16.0f};
};

} // namespace sirpg::components

#endif // SIRPG_COMPONENTS_HPP
