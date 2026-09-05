#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/operation.h"

namespace {

using sound_mind::core::Operation;
using sound_mind::core::OperationId;
using sound_mind::core::TimeFrequencyRect;

/// Minimal concrete Operation for exercising the base class in isolation -
/// Operation itself has no real subtypes yet (Paint/Filter/Generator/
/// Transform/structural ones are future work).
class FakeOperation final : public Operation {
public:
    // Not `using Operation::Operation;` - inherited constructors keep the
    // base's access level (protected here), regardless of where the
    // using-declaration itself appears, so that wouldn't be callable from
    // outside the class either. An explicit forwarding constructor is
    // needed to actually make construction public for this concrete type.
    explicit FakeOperation(OperationId id, std::optional<OperationId> supersedes = std::nullopt)
        : Operation(id, supersedes) {}

    [[nodiscard]] TimeFrequencyRect bounds() const override {
        return TimeFrequencyRect{0.0, 1.0, 20.0, 20000.0};
    }
};

}  // namespace

TEST_CASE("Operation reports the id it was constructed with", "[core][operation]") {
    const FakeOperation op(42);
    REQUIRE(op.id() == OperationId{42});
}

TEST_CASE("Operation has no supersedes reference unless one is given", "[core][operation]") {
    const FakeOperation op(1);
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("Operation can record which prior operation it supersedes", "[core][operation]") {
    const FakeOperation op(2, OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("A concrete Operation subtype reports its own bounds", "[core][operation]") {
    const FakeOperation op(1);
    const TimeFrequencyRect bounds = op.bounds();
    REQUIRE(bounds.startTimeSeconds == 0.0);
    REQUIRE(bounds.endTimeSeconds == 1.0);
    REQUIRE(bounds.lowFrequencyHz == 20.0);
    REQUIRE(bounds.highFrequencyHz == 20000.0);
}
