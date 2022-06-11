package drpmesh

import (
	"reflect"
)

// GetKeys returns the list of keys from a map as a slice
func GetKeys(inputMap interface{}) []string {
	s := reflect.ValueOf(inputMap)
	mapKeys := s.MapKeys()
	returnKeys := make([]string, len(mapKeys))
	for i := 0; i < len(mapKeys); i++ {
		switch mapKeys[i].Interface().(type) {
		case string:
			returnKeys[i] = mapKeys[i].Interface().(string)
		}
	}
	return returnKeys
}
