#include "storage/heap/heap_page.h"
#include "storage/tuple/tuple_codec.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <variant>
#include <vector>

namespace dblusblus {
namespace {

constexpr std::array ALL_PHYSICAL_TYPES{
    PhysicalType::BOOLEAN,
    PhysicalType::INT32,
    PhysicalType::INT64,
    PhysicalType::FLOAT64,
    PhysicalType::DATE,
    PhysicalType::TIMESTAMP,
    PhysicalType::VARCHAR,
};

template <typename Value>
[[nodiscard]] const Value* RequireOptional(const std::optional<Value>& optional,
                                           std::size_t iteration,
                                           std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing at iteration " << iteration;
        return nullptr;
    }
    return std::addressof(*optional);
}

[[nodiscard]] TupleValue RandomFixedValue(PhysicalType type, std::mt19937_64& random) {
    switch (type) {
    case PhysicalType::BOOLEAN:
        return TupleValue{(random() & 1U) != 0};
    case PhysicalType::INT32:
        return TupleValue{std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random()))};
    case PhysicalType::INT64:
        return TupleValue{std::bit_cast<std::int64_t>(random())};
    case PhysicalType::FLOAT64:
        return TupleValue{Float64PhysicalValue{.bits = random()}};
    case PhysicalType::DATE:
        return TupleValue{
            DatePhysicalValue{
                .value = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random())),
            },
        };
    case PhysicalType::TIMESTAMP:
        return TupleValue{
            TimestampPhysicalValue{.value = std::bit_cast<std::int64_t>(random())},
        };
    case PhysicalType::VARCHAR:
        break;
    }
    return TupleValue{std::monostate{}};
}

TEST(TupleCodecPropertyTest, RandomizedMixedSchemasRoundTripCanonicalBytes) {
    // The fixed seed makes this property test deterministic and reproducible.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937_64 random{0xDB10020ULL};
    std::uniform_int_distribution<std::size_t> column_count_distribution{0, 12};
    std::uniform_int_distribution<std::size_t> type_distribution{0, ALL_PHYSICAL_TYPES.size() - 1U};
    std::uniform_int_distribution<std::size_t> varchar_length_distribution{0, 32};

    constexpr std::size_t iteration_count = 2000;
    for (std::size_t iteration = 0; iteration < iteration_count; ++iteration) {
        const std::size_t column_count = column_count_distribution(random);
        std::vector<PhysicalColumnSpec> columns;
        columns.reserve(column_count);
        std::vector<std::vector<std::byte>> varchar_storage(column_count);
        std::vector<TupleValue> expected_values;
        expected_values.reserve(column_count);

        for (std::size_t column_index = 0; column_index < column_count; ++column_index) {
            const PhysicalType type = ALL_PHYSICAL_TYPES[type_distribution(random)];
            const bool nullable = (random() & 1U) != 0;
            columns.push_back({.type = type, .nullable = nullable});

            const bool is_null = nullable && (random() % 4U == 0);
            if (is_null) {
                expected_values.emplace_back(std::monostate{});
                continue;
            }
            if (type != PhysicalType::VARCHAR) {
                expected_values.push_back(RandomFixedValue(type, random));
                continue;
            }

            auto& bytes = varchar_storage[column_index];
            bytes.resize(varchar_length_distribution(random));
            for (auto& byte : bytes) {
                byte = static_cast<std::byte>(random() & 0xFFU);
            }
            expected_values.emplace_back(VarcharValue{.bytes = bytes});
        }

        const auto schema_version = static_cast<SchemaVer>(random());
        const auto built = BuildTuplePhysicalLayout(columns, schema_version);
        const auto* layout = RequireOptional(built.layout, iteration, "physical layout");
        ASSERT_NE(layout, nullptr);

        const bool has_previous = (random() & 1U) != 0;
        auto previous_page_no = static_cast<PageNo>(random());
        if (previous_page_no == 0 || previous_page_no == INVALID_PAGE_NO) {
            previous_page_no = 1;
        }
        auto previous_slot = static_cast<SlotId>(random());
        if (previous_slot == INVALID_SLOT_ID) {
            previous_slot = static_cast<SlotId>(INVALID_SLOT_ID - 1U);
        }
        const TupleVersionMetadata metadata{
            .xmin = random(),
            .xmax = random(),
            .cmin = static_cast<CommandId>(random()),
            .cmax = static_cast<CommandId>(random()),
            .prev_page_no = has_previous ? previous_page_no : INVALID_PAGE_NO,
            .prev_slot = has_previous ? previous_slot : INVALID_SLOT_ID,
        };

        const auto encoded = EncodeTuple(*layout, metadata, expected_values);
        const auto* tuple = RequireOptional(encoded.tuple, iteration, "encoded tuple");
        ASSERT_NE(tuple, nullptr);
        EXPECT_LE(tuple->size(), HEAP_PAGE_MAX_RAW_TUPLE_SIZE) << "iteration " << iteration;

        const auto validation = ValidateTuple(*layout, *tuple);
        ASSERT_TRUE(validation) << "iteration " << iteration;
        const auto* header = RequireOptional(validation.header, iteration, "validated header");
        ASSERT_NE(header, nullptr);
        EXPECT_EQ(header->schema_version, schema_version) << "iteration " << iteration;

        std::vector<TupleValue> decoded_values;
        decoded_values.reserve(column_count);
        for (std::size_t column_index = 0; column_index < column_count; ++column_index) {
            const auto decoded = DecodeTupleValue(*layout, *tuple, column_index);
            const auto* decoded_value =
                RequireOptional(decoded.value, iteration, "decoded tuple value");
            ASSERT_NE(decoded_value, nullptr) << "column " << column_index;
            EXPECT_EQ(*decoded_value, expected_values[column_index])
                << "iteration " << iteration << ", column " << column_index;
            decoded_values.push_back(*decoded_value);
        }

        const auto reencoded = EncodeTuple(*layout, metadata, decoded_values);
        const auto* reencoded_tuple =
            RequireOptional(reencoded.tuple, iteration, "re-encoded tuple");
        ASSERT_NE(reencoded_tuple, nullptr);
        EXPECT_EQ(*reencoded_tuple, *tuple) << "iteration " << iteration;
    }
}

} // namespace
} // namespace dblusblus
