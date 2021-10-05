package drpmesh

// UMLAttribute is assigned to UMLClass
type UMLAttribute struct {
	Name         string
	Stereotype   string
	Visibility   string
	Derived      bool
	Type         string
	Default      string
	Multiplicity string
	Restrictions []string
}

// UMLFunction is assigned to UMLClass
type UMLFunction struct {
	Name       string
	Visibility string
	Parameters []string
	Return     string
}

// UMLClass declares data structures offered by a service
type UMLClass struct {
	Name        string
	Stereotypes string
	PrimaryKey  string
	Attributes  map[string]UMLAttribute
	Functions   map[string]UMLFunction
}

// GetPK returns the primary key field of a class
func (uc UMLClass) GetPK() *string {
	for attributeName, attributeObject := range uc.Attributes {
		for _, restriction := range attributeObject.Restrictions {
			if restriction == "PK" {
				return &attributeName
			}
		}
	}
	return nil
}

/*
TO DO - ADD THESE METHODS
GetRecords - hold off, may want to separate this from the class definition itself
GetDefinition - hold off, may just return class definition to marshal to JSON if we're separating the records
AddRecord - hold off, see previous lines

Historical Note
-----------------
The original implementation (~2015) was a caching system which loaded all records from the
source into memory.  The UMLClass object had a .cache attribute where these records were
stored.  This was geared toward small, infrastructure related data sets (<500MB) and did
not scale well.  Going forward the UMLClass should simply be used for advertising and
discoverability as opposed to holding any records itself, even temporarily.
*/
