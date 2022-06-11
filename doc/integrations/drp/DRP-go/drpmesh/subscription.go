package drpmesh

// SubscribableSourceInterface should be implemented by a SubscribableSource
type SubscribableSourceInterface interface {
	AddSubscription(Subscriber)
	RemoveSubscription(Subscriber)
	Send(interface{})
}

// SubscribableSource applies attributes to subscribable sources
type SubscribableSource struct {
	NodeID        string
	TopicName     string
	Subscriptions []Subscriber
}

// Subscriber contains details sent by a process which needs data from a SubscribableSource
type Subscriber struct{}

// Send forwards subscribed data to a Subscriber
func (s Subscriber) Send(interface{}) {}

// Terminate removes open subscriptions
func (s Subscriber) Terminate() {}

// RemoteSubscription is a subscription to a remote source
type RemoteSubscription struct {
	SubscribableSource
	StreamToken           string
	NoSubscribersCallback interface{}
}

// RemoveSubscription removes a local subscription
func (rs RemoteSubscription) RemoveSubscription(subscription Subscriber) {

}

// SubscriptionManager is used to deduplicate multiple subscriptions from local clients to a remote source
type SubscriptionManager struct{}
