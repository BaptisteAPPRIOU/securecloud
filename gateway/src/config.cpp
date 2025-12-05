#include "config.hpp"
#include "httpServer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <stdexcept>

namespace gateway {

static inline std::string trim_str(const std::string &s) {
    const auto strBegin = s.find_first_not_of(" \t\r\n");
    if (strBegin == std::string::npos) return "";
    const auto strEnd = s.find_last_not_of(" \t\r\n");
    return s.substr(strBegin, strEnd - strBegin + 1);
}

static inline std::string unquote(const std::string& s) {
    std::string v = trim_str(s);
    if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'')) {
        v = v.substr(1, v.size() - 2);
    }
    return v;
}

/**
 * Expand environment variables in configuration values.
 * Supports ${VAR_NAME} syntax. Throws runtime_error if variable is not set.
 * 
 * @param value String that may contain ${VAR_NAME} syntax
 * @return Expanded string with environment variable substituted
 * @throws std::runtime_error if environment variable is not set
 */
static inline std::string expand_env_vars(const std::string& value) {
    std::string result = value;
    size_t pos = 0;
    
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            throw std::runtime_error("Malformed environment variable syntax: missing closing '}' in: " + value);
        }
        
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        const char* env_value = std::getenv(var_name.c_str());
        
        if (env_value == nullptr) {
            throw std::runtime_error(
                "Environment variable '" + var_name + "' is not set. " +
                "Please set it before starting the gateway (e.g., export " + var_name + "=<value>)"
            );
        }
        
        result.replace(pos, end_pos - pos + 1, env_value);
        pos += std::strlen(env_value);
    }
    
    return result;
}

static inline int get_indent(const std::string& line) {
    size_t pos = line.find_first_not_of(' ');
    return pos == std::string::npos ? 0 : static_cast<int>(pos);
}

static inline bool parse_bool(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "yes" || lower == "1";
}

/**
 * Simple YAML parser for gateway configuration.
 * NOTE: This is a minimal parser - for production, consider using yaml-cpp library.
 * Current implementation supports:
 * - Nested dictionaries (by indentation)
 * - Lists (- prefix)
 * - String/int/bool values
 * 
 * Limitations:
 * - No support for multiline strings, anchors, complex YAML features
 * - Limited error handling
 */
class SimpleYAMLParser {
public:
    explicit SimpleYAMLParser(std::ifstream& file) : file_(file) {}

    GatewayConfig parse() {
        GatewayConfig config;
        std::string line;
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            int indent = get_indent(line);
            
            // Top-level sections
            if (indent == 0) {
                if (trimmed.rfind("server:", 0) == 0) {
                    parse_server_section(config.server);
                } else if (trimmed.rfind("routing:", 0) == 0) {
                    parse_routing_section(config.routes);
                } else if (trimmed.rfind("upstreams:", 0) == 0) {
                    parse_upstreams_section(config.upstreams);
                } else if (trimmed.rfind("security:", 0) == 0) {
                    parse_security_section(config.security);
                } else if (trimmed.rfind("rate_limits:", 0) == 0) {
                    parse_rate_limits_section(config.rate_limits);
                } else if (trimmed.rfind("observability:", 0) == 0) {
                    parse_observability_section(config.observability);
                }
            }
        }
        
        return config;
    }

private:
    std::ifstream& file_;
    
    void parse_server_section(ServerConfig& server) {
        std::string line;
        long pos = file_.tellg();
        TLSConfig tls_cfg;
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                return;
            }
            
            if (indent == 2) {
                if (trimmed.rfind("host:", 0) == 0) {
                    server.host = unquote(trimmed.substr(5));
                } else if (trimmed.rfind("port:", 0) == 0) {
                    try {
                        server.port = std::stoi(trim_str(trimmed.substr(5)));
                    } catch (...) {}
                } else if (trimmed.rfind("tls:", 0) == 0) {
                    // Parse TLS subsection (indented at level 4)
                    long tls_pos = file_.tellg();
                    while (std::getline(file_, line)) {
                        trimmed = trim_str(line);
                        if (trimmed.empty()) continue;
                        
                        int tls_indent = get_indent(line);
                        if (tls_indent <= 2) {
                            file_.seekg(tls_pos);
                            break;
                        }
                        
                        if (tls_indent == 4) {
                            if (trimmed.rfind("cert_file:", 0) == 0) {
                                tls_cfg.cert_file = unquote(trimmed.substr(10));
                            } else if (trimmed.rfind("key_file:", 0) == 0) {
                                tls_cfg.key_file = unquote(trimmed.substr(9));
                            } else if (trimmed.rfind("client_mtls:", 0) == 0) {
                                tls_cfg.client_mtls = parse_bool(trim_str(trimmed.substr(12)));
                            }
                        }
                        tls_pos = file_.tellg();
                    }
                }
            }
            pos = file_.tellg();
        }
    }
    
    void parse_routing_section(std::vector<RouteConfig>& routes) {
        std::string line;
        long pos = file_.tellg();
        RouteConfig current_route;
        bool in_route = false;
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                if (in_route && !current_route.match_pattern.empty()) {
                    routes.push_back(current_route);
                }
                return;
            }
            
            if (indent == 2 && trimmed[0] == '-') {
                // New route entry
                if (in_route && !current_route.match_pattern.empty()) {
                    routes.push_back(current_route);
                }
                current_route = RouteConfig{};
                in_route = true;
                
                // Check if match is on same line
                std::string rest = trim_str(trimmed.substr(1));
                if (rest.rfind("match:", 0) == 0) {
                    current_route.match_pattern = unquote(rest.substr(6));
                }
            } else if (indent == 4 && in_route) {
                if (trimmed.rfind("match:", 0) == 0) {
                    current_route.match_pattern = unquote(trimmed.substr(6));
                } else if (trimmed.rfind("target:", 0) == 0) {
                    current_route.target = unquote(trimmed.substr(7));
                } else if (trimmed.rfind("upgrade:", 0) == 0) {
                    std::string upgrade_val = unquote(trimmed.substr(8));
                    current_route.upgrade_websocket = (upgrade_val == "websocket");
                }
            }
            pos = file_.tellg();
        }
        
        if (in_route && !current_route.match_pattern.empty()) {
            routes.push_back(current_route);
        }
    }
    
    void parse_upstreams_section(std::vector<UpstreamConfig>& upstreams) {
        std::string line;
        long pos = file_.tellg();
        std::string current_name;
        UpstreamConfig current_upstream;
        bool in_upstream = false;
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                if (in_upstream && !current_name.empty()) {
                    current_upstream.name = current_name;
                    upstreams.push_back(current_upstream);
                }
                return;
            }
            
            if (indent == 2 && trimmed.find(':') != std::string::npos) {
                // New upstream entry (e.g., "auth:")
                if (in_upstream && !current_name.empty()) {
                    current_upstream.name = current_name;
                    upstreams.push_back(current_upstream);
                }
                
                current_name = trimmed.substr(0, trimmed.find(':'));
                current_upstream = UpstreamConfig{};
                in_upstream = true;
            } else if (indent == 4 && in_upstream) {
                if (trimmed.rfind("kind:", 0) == 0) {
                    std::string kind = unquote(trimmed.substr(5));
                    if (kind == "ws") current_upstream.kind = UpstreamKind::WS;
                    else current_upstream.kind = UpstreamKind::HTTP;
                } else if (trimmed.rfind("transport:", 0) == 0) {
                    std::string transport = unquote(trimmed.substr(10));
                    if (transport == "uds") current_upstream.transport = UpstreamTransport::UDS;
                    else current_upstream.transport = UpstreamTransport::TCP;
                } else if (trimmed.rfind("socket:", 0) == 0) {
                    current_upstream.address = expand_env_vars(unquote(trimmed.substr(7)));
                } else if (trimmed.rfind("address:", 0) == 0) {
                    current_upstream.address = expand_env_vars(unquote(trimmed.substr(8)));
                }
            }
            pos = file_.tellg();
        }
        
        if (in_upstream && !current_name.empty()) {
            current_upstream.name = current_name;
            upstreams.push_back(current_upstream);
        }
    }
    
    void parse_security_section(SecurityConfig& security) {
        std::string line;
        long pos = file_.tellg();
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                return;
            }
            
            if (indent == 2 && trimmed.rfind("jwt_secret:", 0) == 0) {
                security.jwt_secret = expand_env_vars(unquote(trimmed.substr(11)));
            } else if (indent == 2 && trimmed.rfind("jwks:", 0) == 0) {
                // Parse JWKS subsection
                long jwks_pos = file_.tellg();
                while (std::getline(file_, line)) {
                    trimmed = trim_str(line);
                    if (trimmed.empty()) continue;
                    
                    int jwks_indent = get_indent(line);
                    if (jwks_indent <= 2) {
                        file_.seekg(jwks_pos);
                        break;
                    }
                    
                    if (jwks_indent == 4) {
                        if (trimmed.rfind("cache_ttl_s:", 0) == 0) {
                            try {
                                security.jwks_cache_ttl_s = std::stoi(trim_str(trimmed.substr(12)));
                            } catch (...) {}
                        }
                        // TODO: Parse providers array when needed
                    }
                    jwks_pos = file_.tellg();
                }
            }
            pos = file_.tellg();
        }
    }
    
    void parse_rate_limits_section(RateLimitConfig& rate_limits) {
        std::string line;
        long pos = file_.tellg();
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                return;
            }
            
            if (indent == 2) {
                if (trimmed.rfind("enabled:", 0) == 0) {
                    rate_limits.enabled = parse_bool(trim_str(trimmed.substr(8)));
                } else if (trimmed.rfind("global_capacity:", 0) == 0) {
                    try {
                        rate_limits.global_capacity = std::stoul(trim_str(trimmed.substr(16)));
                    } catch (...) {}
                } else if (trimmed.rfind("global_refill_rate:", 0) == 0) {
                    try {
                        rate_limits.global_refill_rate = std::stod(trim_str(trimmed.substr(19)));
                    } catch (...) {}
                } else if (trimmed.rfind("ip_capacity:", 0) == 0) {
                    try {
                        rate_limits.ip_capacity = std::stoul(trim_str(trimmed.substr(12)));
                    } catch (...) {}
                } else if (trimmed.rfind("ip_refill_rate:", 0) == 0) {
                    try {
                        rate_limits.ip_refill_rate = std::stod(trim_str(trimmed.substr(15)));
                    } catch (...) {}
                } else if (trimmed.rfind("user_capacity:", 0) == 0) {
                    try {
                        rate_limits.user_capacity = std::stoul(trim_str(trimmed.substr(14)));
                    } catch (...) {}
                } else if (trimmed.rfind("user_refill_rate:", 0) == 0) {
                    try {
                        rate_limits.user_refill_rate = std::stod(trim_str(trimmed.substr(17)));
                    } catch (...) {}
                } else if (trimmed.rfind("endpoint_capacity:", 0) == 0) {
                    try {
                        rate_limits.endpoint_capacity = std::stoul(trim_str(trimmed.substr(18)));
                    } catch (...) {}
                } else if (trimmed.rfind("endpoint_refill_rate:", 0) == 0) {
                    try {
                        rate_limits.endpoint_refill_rate = std::stod(trim_str(trimmed.substr(21)));
                    } catch (...) {}
                }
            }
            pos = file_.tellg();
        }
    }
    
    void parse_observability_section(ObservabilityConfig& obs) {
        std::string line;
        long pos = file_.tellg();
        
        while (std::getline(file_, line)) {
            std::string trimmed = trim_str(line);
            if (trimmed.empty()) continue;
            
            int indent = get_indent(line);
            if (indent == 0) {
                file_.seekg(pos);
                return;
            }
            
            if (indent == 2) {
                if (trimmed.rfind("prometheus:", 0) == 0) {
                    // Parse Prometheus subsection
                    long prom_pos = file_.tellg();
                    while (std::getline(file_, line)) {
                        trimmed = trim_str(line);
                        if (trimmed.empty()) continue;
                        
                        int prom_indent = get_indent(line);
                        if (prom_indent <= 2) {
                            file_.seekg(prom_pos);
                            break;
                        }
                        
                        if (prom_indent == 4 && trimmed.rfind("bind:", 0) == 0) {
                            obs.prometheus.bind_address = unquote(trimmed.substr(5));
                        }
                        prom_pos = file_.tellg();
                    }
                } else if (trimmed.rfind("logs:", 0) == 0) {
                    // Parse Logs subsection
                    long log_pos = file_.tellg();
                    while (std::getline(file_, line)) {
                        trimmed = trim_str(line);
                        if (trimmed.empty()) continue;
                        
                        int log_indent = get_indent(line);
                        if (log_indent <= 2) {
                            file_.seekg(log_pos);
                            break;
                        }
                        
                        if (log_indent == 4 && trimmed.rfind("level:", 0) == 0) {
                            obs.logs.level = unquote(trimmed.substr(6));
                        }
                        log_pos = file_.tellg();
                    }
                }
            }
            pos = file_.tellg();
        }
    }
};

GatewayConfig load_gateway_config(const std::string& path, const std::string& env) {
    GatewayConfig config;
    config.environment = env;
    
    // Try to load environment-specific file first (e.g., gateway.prod.yaml)
    std::string env_path = path;
    if (env != "dev") {
        size_t pos = path.rfind(".dev.");
        if (pos != std::string::npos) {
            env_path = path.substr(0, pos) + "." + env + path.substr(pos + 4);
        }
    }
    
    std::ifstream file(env_path);
    if (!file.is_open()) {
        if (env_path != path) {
            // Fallback to default path
            file.open(path);
        }
        if (!file.is_open()) {
            spdlog::warn("Config file not found at '{}', using defaults", path);
            // Set default values
            config.server.host = "127.0.0.1";
            config.server.port = 8080;
            return config;
        }
    }
    
    spdlog::info("Loading gateway config from: {}", env_path != path ? env_path : path);
    
    SimpleYAMLParser parser(file);
    config = parser.parse();
    
    // Set defaults for server if not parsed
    if (config.server.host.empty()) config.server.host = "127.0.0.1";
    if (config.server.port == 0) config.server.port = 8080;
    
    // Log loaded configuration
    spdlog::info("Gateway config loaded - Server: {}:{}, Routes: {}, Upstreams: {}",
                 config.server.host, config.server.port,
                 config.routes.size(), config.upstreams.size());
    
    if (!config.routes.empty()) {
        spdlog::debug("Loaded {} routing rules", config.routes.size());
        for (const auto& route : config.routes) {
            spdlog::debug("  Route: {} -> {} (WS: {})",
                         route.match_pattern, route.target, route.upgrade_websocket);
        }
    }
    
    if (!config.upstreams.empty()) {
        spdlog::debug("Loaded {} upstreams", config.upstreams.size());
        for (const auto& upstream : config.upstreams) {
            spdlog::debug("  Upstream '{}': {} via {} at {}",
                         upstream.name,
                         upstream.kind == UpstreamKind::WS ? "WebSocket" : "HTTP",
                         upstream.transport == UpstreamTransport::UDS ? "UDS" : "TCP",
                         upstream.address);
        }
    }
    
    return config;
}

// Legacy function for backward compatibility
ServerConfig load_server_config(const std::string& path) {
    std::ifstream in(path);
    ServerConfig config;
    
    if (!in.is_open()) {
        spdlog::warn("Config file not found at '{}', using defaults", path);
        return config;
    }

    std::string line;
    bool in_server = false;
    int server_indent = -1;
    while (std::getline(in, line)) {
        std::string raw = line;
        std::string t = trim_str(raw);
        if (t.empty()) continue;

        size_t indent_pos = raw.find_first_not_of(' ');
        int indent = indent_pos == std::string::npos ? 0 : static_cast<int>(indent_pos);

        if (!in_server) {
            if (t.rfind("server:", 0) == 0) {
                in_server = true;
                server_indent = indent;
            }
            continue;
        }

        // If we left the server block
        if (indent <= server_indent) break;

        if (t.rfind("host:", 0) == 0) {
            std::string v = trim_str(t.substr(5));
            if (!v.empty() && (v.front() == '"' || v.front() == '\'')) {
                if (v.size() >= 2) v = v.substr(1, v.size() - 2);
            }
            if (!v.empty()) config.host = v;
        } else if (t.rfind("port:", 0) == 0) {
            std::string v = trim_str(t.substr(5));
            try {
                config.port = std::stoi(v);
            } catch (...) {
            }
        }
    }

    spdlog::info("Loaded server config from '{}': {}:{}", path, config.host, config.port);
    return config;
}

}  // namespace gateway
