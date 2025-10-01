// Message schema definitions and type validation

/**
 * WebSocket message types
 */
export const MessageTypes = {
  // Data messages
  CLUSTERS_LITE: 'clusters-lite',
  RAW_LITE: 'raw-lite',
  FILTERED_LITE: 'filtered-lite',
  
  // Sensor messages
  SENSOR_SNAPSHOT: 'sensor.snapshot',
  SENSOR_UPDATED: 'sensor.updated',
  SENSOR_UPDATE: 'sensor.update',
  SENSOR_ENABLE: 'sensor.enable',
  SENSOR_REQUEST_SNAPSHOT: 'sensor.requestSnapshot',
  
  // Filter messages
  FILTER_CONFIG: 'filter.config',
  FILTER_UPDATED: 'filter.updated',
  FILTER_UPDATE: 'filter.update',
  FILTER_REQUEST_CONFIG: 'filter.requestConfig',
  
  // DBSCAN messages
  DBSCAN_CONFIG: 'dbscan.config',
  DBSCAN_UPDATED: 'dbscan.updated',
  DBSCAN_UPDATE: 'dbscan.update',
  DBSCAN_REQUEST_CONFIG: 'dbscan.requestConfig',
  
  // World/ROI messages
  WORLD_UPDATED: 'world.updated',
  WORLD_UPDATE: 'world.update',
  
  // Generic response messages
  OK: 'ok',
  ERROR: 'error'
};

/**
 * Sensor types
 */
export const SensorTypes = {
  HOKUYO_URG_ETH: 'hokuyo_urg_eth',
  UNKNOWN: 'unknown'
};

/**
 * Sensor modes
 */
export const SensorModes = {
  ME: 'ME', // Distance + Intensity
  MD: 'MD'  // Distance Only
};

/**
 * Sink types
 */
export const SinkTypes = {
  NNG: 'nng',
  OSC: 'osc'
};

/**
 * Encoding types for NNG sinks
 */
export const EncodingTypes = {
  MSGPACK: 'msgpack',
  JSON: 'json'
};

/**
 * ROI types
 */
export const ROITypes = {
  INCLUDE: 'include',
  EXCLUDE: 'exclude'
};

/**
 * Drag modes for canvas interaction
 */
export const DragModes = {
  PAN: 'pan',
  SENSOR: 'sensor',
  ROI: 'roi',
  VERTEX: 'vertex'
};

/**
 * ROI edit modes
 */
export const ROIEditModes = {
  NONE: 'none',
  CREATE_INCLUDE: 'create_include',
  CREATE_EXCLUDE: 'create_exclude',
  EDIT: 'edit'
};

/**
 * Connection states
 */
export const ConnectionStates = {
  CONNECTING: 'connecting',
  CONNECTED: 'connected',
  CLOSING: 'closing',
  DISCONNECTED: 'disconnected',
  ERROR: 'error'
};

/**
 * Notification types
 */
export const NotificationTypes = {
  INFO: 'info',
  SUCCESS: 'success',
  WARNING: 'warning',
  ERROR: 'error'
};

/**
 * Validate a WebSocket message structure
 * @param {any} message - The message to validate
 * @returns {boolean} True if message is valid
 */
export function validateMessage(message) {
  if (!message || typeof message !== 'object') {
    return false;
  }
  
  if (!message.type || typeof message.type !== 'string') {
    return false;
  }
  
  return Object.values(MessageTypes).includes(message.type);
}

/**
 * Validate sensor data structure
 * @param {any} sensor - The sensor data to validate
 * @returns {boolean} True if sensor data is valid
 */
export function validateSensor(sensor) {
  if (!sensor || typeof sensor !== 'object') {
    return false;
  }
  
  // Required fields
  if (typeof sensor.id === 'undefined') {
    return false;
  }
  
  // Optional but validated fields
  if (sensor.type && !Object.values(SensorTypes).includes(sensor.type)) {
    return false;
  }
  
  if (sensor.mode && !Object.values(SensorModes).includes(sensor.mode)) {
    return false;
  }
  
  if (sensor.enabled !== undefined && typeof sensor.enabled !== 'boolean') {
    return false;
  }
  
  // Validate pose if present
  if (sensor.pose) {
    if (typeof sensor.pose !== 'object') {
      return false;
    }
    
    const { tx, ty, theta_deg } = sensor.pose;
    if (tx !== undefined && typeof tx !== 'number') return false;
    if (ty !== undefined && typeof ty !== 'number') return false;
    if (theta_deg !== undefined && typeof theta_deg !== 'number') return false;
  }
  
  return true;
}

/**
 * Validate sink data structure
 * @param {any} sink - The sink data to validate
 * @returns {boolean} True if sink data is valid
 */
export function validateSink(sink) {
  if (!sink || typeof sink !== 'object') {
    return false;
  }
  
  // Required fields
  if (!sink.type || !Object.values(SinkTypes).includes(sink.type)) {
    return false;
  }
  
  if (typeof sink.enabled !== 'boolean') {
    return false;
  }
  
  // Type-specific validation
  if (sink.type === SinkTypes.NNG) {
    if (sink.encoding && !Object.values(EncodingTypes).includes(sink.encoding)) {
      return false;
    }
  }
  
  if (sink.type === SinkTypes.OSC) {
    if (sink.in_bundle !== undefined && typeof sink.in_bundle !== 'boolean') {
      return false;
    }
    if (sink.bundle_fragment_size !== undefined && typeof sink.bundle_fragment_size !== 'number') {
      return false;
    }
  }
  
  // Optional numeric fields
  if (sink.rate_limit !== undefined && typeof sink.rate_limit !== 'number') {
    return false;
  }
  
  return true;
}

/**
 * Validate filter configuration structure
 * @param {any} config - The filter config to validate
 * @returns {boolean} True if config is valid
 */
export function validateFilterConfig(config) {
  if (!config || typeof config !== 'object') {
    return false;
  }
  
  // Validate prefilter
  if (config.prefilter) {
    if (typeof config.prefilter !== 'object') return false;
    if (typeof config.prefilter.enabled !== 'boolean') return false;
    
    // Validate individual filter strategies
    const strategies = ['neighborhood', 'spike_removal', 'outlier_removal', 'intensity_filter', 'isolation_removal'];
    for (const strategy of strategies) {
      if (config.prefilter[strategy]) {
        if (typeof config.prefilter[strategy] !== 'object') return false;
        if (typeof config.prefilter[strategy].enabled !== 'boolean') return false;
      }
    }
  }
  
  // Validate postfilter
  if (config.postfilter) {
    if (typeof config.postfilter !== 'object') return false;
    if (typeof config.postfilter.enabled !== 'boolean') return false;
    
    if (config.postfilter.isolation_removal) {
      if (typeof config.postfilter.isolation_removal !== 'object') return false;
      if (typeof config.postfilter.isolation_removal.enabled !== 'boolean') return false;
    }
  }
  
  return true;
}

/**
 * Validate DBSCAN configuration structure
 * @param {any} config - The DBSCAN config to validate
 * @returns {boolean} True if config is valid
 */
export function validateDbscanConfig(config) {
  if (!config || typeof config !== 'object') {
    return false;
  }
  
  const requiredFields = ['eps_norm', 'minPts', 'k_scale', 'h_min', 'h_max', 'R_max', 'M_max'];
  
  for (const field of requiredFields) {
    if (config[field] === undefined || typeof config[field] !== 'number') {
      return false;
    }
  }
  
  // Validate ranges
  if (config.eps_norm <= 0) return false;
  if (config.minPts < 1) return false;
  if (config.k_scale <= 0) return false;
  if (config.h_min <= 0) return false;
  if (config.h_max <= config.h_min) return false;
  if (config.R_max < 1) return false;
  if (config.M_max < 1) return false;
  
  return true;
}

/**
 * Validate world mask structure
 * @param {any} worldMask - The world mask to validate
 * @returns {boolean} True if world mask is valid
 */
export function validateWorldMask(worldMask) {
  if (!worldMask || typeof worldMask !== 'object') {
    return false;
  }
  
  // Validate includes array
  if (worldMask.includes) {
    if (!Array.isArray(worldMask.includes)) return false;
    for (const polygon of worldMask.includes) {
      if (!validatePolygon(polygon)) return false;
    }
  }
  
  // Validate excludes array
  if (worldMask.excludes) {
    if (!Array.isArray(worldMask.excludes)) return false;
    for (const polygon of worldMask.excludes) {
      if (!validatePolygon(polygon)) return false;
    }
  }
  
  return true;
}

/**
 * Validate polygon structure
 * @param {any} polygon - The polygon to validate
 * @returns {boolean} True if polygon is valid
 */
export function validatePolygon(polygon) {
  if (!Array.isArray(polygon)) return false;
  if (polygon.length < 3) return false; // Need at least 3 points for a polygon
  
  for (const point of polygon) {
    if (!Array.isArray(point) || point.length !== 2) return false;
    if (typeof point[0] !== 'number' || typeof point[1] !== 'number') return false;
  }
  
  return true;
}

/**
 * Validate viewport structure
 * @param {any} viewport - The viewport to validate
 * @returns {boolean} True if viewport is valid
 */
export function validateViewport(viewport) {
  if (!viewport || typeof viewport !== 'object') {
    return false;
  }
  
  const { x, y, scale } = viewport;
  
  if (typeof x !== 'number' || typeof y !== 'number' || typeof scale !== 'number') {
    return false;
  }
  
  if (scale <= 0) return false;
  
  return true;
}

/**
 * Create a default sensor object
 * @param {string} id - Sensor ID
 * @returns {Object} Default sensor object
 */
export function createDefaultSensor(id) {
  return {
    id,
    type: SensorTypes.HOKUYO_URG_ETH,
    enabled: true,
    pose: {
      tx: 0,
      ty: 0,
      theta_deg: 0
    },
    endpoint: {
      host: '192.168.1.10',
      port: 10940
    },
    mode: SensorModes.ME
  };
}

/**
 * Create a default sink object
 * @param {string} type - Sink type
 * @returns {Object} Default sink object
 */
export function createDefaultSink(type = SinkTypes.NNG) {
  const base = {
    type,
    enabled: true,
    url: type === SinkTypes.NNG ? 'tcp://localhost:5555' : 'udp://localhost:8000',
    topic: 'lidar_data',
    rate_limit: 30
  };
  
  if (type === SinkTypes.NNG) {
    base.encoding = EncodingTypes.MSGPACK;
  } else if (type === SinkTypes.OSC) {
    base.in_bundle = false;
    base.bundle_fragment_size = 1024;
  }
  
  return base;
}

/**
 * Create a default filter configuration
 * @returns {Object} Default filter configuration
 */
export function createDefaultFilterConfig() {
  return {
    prefilter: {
      enabled: true,
      neighborhood: { enabled: true, k: 5, r_base: 0.05, r_scale: 1.0 },
      spike_removal: { enabled: true, dr_threshold: 0.3, window_size: 3 },
      outlier_removal: { enabled: true, median_window: 5, outlier_threshold: 2.0 },
      intensity_filter: { enabled: false, min_intensity: 0, min_reliability: 0 },
      isolation_removal: { enabled: true, min_cluster_size: 3, isolation_radius: 0.1 }
    },
    postfilter: {
      enabled: true,
      isolation_removal: { enabled: true, min_points_size: 3, isolation_radius: 0.2, required_neighbors: 2 }
    }
  };
}

/**
 * Create a default DBSCAN configuration
 * @returns {Object} Default DBSCAN configuration
 */
export function createDefaultDbscanConfig() {
  return {
    eps_norm: 2.5,
    minPts: 5,
    k_scale: 1.0,
    h_min: 0.01,
    h_max: 0.20,
    R_max: 5,
    M_max: 600
  };
}

/**
 * Create a default viewport
 * @returns {Object} Default viewport
 */
export function createDefaultViewport() {
  return {
    x: 0,
    y: 0,
    scale: 60
  };
}

/**
 * Create a default world mask
 * @returns {Object} Default world mask
 */
export function createDefaultWorldMask() {
  return {
    include: [],
    exclude: []
  };
}