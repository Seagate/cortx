<template>
  <div>
      <v-container fill-height fluid>
        <br>

  <v-row align="center"
      justify="center" class="mt-4">
      <v-card height="80vh" width="80vw" >
        <v-subheader   style="background-color:#f1f1f1;" >
          <v-row>
            <v-btn to="/dashboard" text icon><v-icon size="30">mdi-arrow-left</v-icon></v-btn>
          </v-row>
      </v-subheader>
      <div class="mt-3" style="overflow:hidden;overflow-y:scroll;height:68vh">

        <div class="ma-8">
            
           <v-simple-table fixed-header >
    
      <thead>
        <tr>
          <th class="text-left">
            Device
          </th>
          <th class="text-left">
            Request
          </th>
          <th class="text-left">
            Response
          </th>
          <th class="text-left">
            Intent Name
          </th>
          <th class="text-left">
            Timestamp
          </th>
          
         
        </tr>
      </thead>
      <tbody>
          <!-- <tr>
              
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="ID"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Request"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Response"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Skill"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Score"></v-text-field>
              </td>
              
              
          </tr> -->
        <tr
          v-for="item in logs"
          :key="item.name"
        >
          
          <td>{{ item.device }}</td>
          <td>{{ item.querytext }}</td>
          <td>{{ item.text }}</td>
          <td>{{ item.skills }}</td>
          <td>{{ item.time }}</td>
          
        </tr>
      </tbody>
   
  </v-simple-table>
  
          </div>
      </div>
      </v-card>
  </v-row>
      </v-container>
  </div>
</template>

<script>
import axios from 'axios';
export default {
    data(){
        return {
            logs:[]
        }
    },
    mounted(){
        
       axios
      .get('https://vue-health-api.herokuapp.com/logs')
      .then(response => {
        this.logs = response.data

        console.log(response.data)
      }).catch(err=>{
        console.log(err)
      })
    
    }
}
</script>

<style>

</style>