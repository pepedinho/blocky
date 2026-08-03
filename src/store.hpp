#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>


/// @brief Represent a stored value along with an optional expiration timestamp.
struct ValueEntry {
  std::string value;
  std::optional<std::chrono::steady_clock::time_point> expire_at = std::nullopt;

  [[nodiscard]] bool is_expired() const noexcept
  {
    if (!expire_at.has_value()) return false;
    return std::chrono::steady_clock::now() >= *expire_at;
  }
};

/// @brief In-memory key/value storage engine.
class Store {
  private:
    std::unordered_map<std::string, ValueEntry> db;

    /// @brief Helper to clean up expired key on access (Lazy Eviction).
    bool purge_if_expired(const std::string& key)
    {
      auto it = db.find(key);
      if (it != db.end() && it->second.is_expired())
      {
        db.erase(it);
        return true;
      }
      return false;
    }

  public:
    Store() = default;

    /// @brief Sets a key-value pair with an optional TTL in seconds.
    void set(std::string_view key, std::string_view value, std::optional<int> ttl_seconds = std::nullopt)
    {
      ValueEntry entry;
      entry.value = std::string(value);
      if (ttl_seconds.has_value() && *ttl_seconds > 0)
      {
        entry.expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(*ttl_seconds);
      }
      else
      {
        entry.expire_at = std::nullopt;
      }
      db[std::string(key)] = std::move(entry);
    }

    /// @brief Gets a value if present and not expired.
    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const noexcept
    {
      std::string k(key);

      auto it = db.find(std::string(k));
      if (it != db.end())
      {
        return std::string_view(it->second.value);
      }
      return std::nullopt;
    }

    /// @brief Sets expiration in seconds for an existing key.
    bool expire(std::string_view key, int seconds)
    {
      std::string k(key);
      if (purge_if_expired(k)) return false;
      
      auto it = db.find(k);
      if (it != db.end())
      {
        it->second.expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        return true;
      }
      return false;
    }
    
    /// @brief Returns remaining TTL in seconds (-2 if non-existant/expired, -1 if no TTL).
    [[nodiscard]] int ttl(std::string_view key)
    {
      std::string k(key);

      if (purge_if_expired(k)) return -2;

      auto it = db.find(k);
      if (it == db.end()) return -2;

      if (!it->second.expire_at.has_value()) return -1;
      auto now = std::chrono::steady_clock::now();
      if (now >= *it->second.expire_at)
      {
        db.erase(it);
        return -2;
      }

      auto remaining = std::chrono::duration_cast<std::chrono::seconds>(*it->second.expire_at - now);
      return static_cast<int>(remaining.count());
    }

    /// @brief Deletes a key.
    bool del(std::string_view key)
    {
      return db.erase(std::string(key)) > 0;
    }

    /// @brief Checks if a key exists and is not expired.
    [[nodiscard]] bool exists(std::string_view key)
    {
      std::string k(key);

      if (purge_if_expired(k)) return false;
      return db.find(k) != db.end();
    }

    [[nodiscard]] size_t size() const noexcept
    {
      return db.size();
    }
};
