// Единицы score фиксированы; radio hops никогда не считаются метрами пути.
class AICF_AdaptiveCommanderConfig
{
	static const float OBJECTIVE_VALUE = 60;
	static const float RELAY_VALUE = 15;
	static const float OPEN_DIRECTION_VALUE = 8;
	static const float CONNECTIVITY_VALUE = 20;
	static const float DISTANCE_COST_PER_METER = 0.015;
	static const float MAX_DISTANCE_COST = 90;
	static const float UNKNOWN_ROUTE_COST = 12;
	static const float DEFICIT_VALUE = 40;
	static const float SATURATION_COST = 60;
	static const float CONTINUATION_VALUE = 18;
	static const float PROGRESS_VALUE = 12;
	static const float LOSS_VALUE = 30;
	static const float HQ_THREAT_VALUE = 200;
	static const float BASE_THREAT_VALUE = 120;
	static const float ATTRITION_COST = 2;
	static const float PROGRESS_METERS = 25;
	static const float ARRIVAL_METERS = 100;
	static const float POINT_COMMIT_RADIUS = 150;
	static const float FRONT_VALUE_PER_EXIT = 5;
	static const float MAX_FRONT_VALUE = 20;
	static const float ATTACK_HOLD_COST = 100;
	static const float SWITCH_MARGIN = 15;
	static const int MIN_ATTACK_ALIVE = 3;
	static const float UNKNOWN_RESISTANCE = 8;
	static const float CONTESTED_DEMAND = 12;
	static const float HQ_DEMAND = 20;
	static const int THREAT_TTL_MS = 15000;
	static const int LOSS_TTL_MS = 90000;
	static const int FAILURE_TTL_MS = 180000;
	static const int TRACE_INTERVAL_MS = 30000;
}
